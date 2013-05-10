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

#include "../../../SubPic/DX9SubPic.h"
#include "RenderersSettings.h"
#include "SyncAllocatorPresenter.h"
#include "AllocatorCommon.h"
#include <D3Dcompiler.h>
#include <mfapi.h>
#include <vmr9.h>
#include <evr9.h>

#define VMRBITMAP_UPDATE 0x80000000
#define MAX_PICTURE_SLOTS (16+2) // Last 2 for pixels shader!
#define NB_JITTER 128// keep this one at a convienient value to allow modulo action by low bitmask

extern bool g_bNoDuration; // Defined in MainFrm.cpp
extern bool g_bExternalSubtitleTime;

namespace GothSync
{
    typedef enum {
        MSG_MIXERIN,
        MSG_MIXEROUT,
        MSG_ERROR
    } EVR_STATS_MSG;

#pragma pack(push, 4)
    template<size_t texcoords>
    struct MYD3DVERTEX {
        float x, y, z, rhw;
        struct {
            float u, v;
        } t[texcoords];
    };
    template<>
    struct MYD3DVERTEX<0> {
        float x, y, z, rhw;
        DWORD Diffuse;
    };
#pragma pack(pop)

    class CGenlock;
    class CSyncRenderer;

    // Base allocator-presenter
    class __declspec(novtable) CBaseAP:
        public CSubPicAllocatorPresenterImpl
    {
    protected:
        // section begin of aligned requirements, operator new has to be overloaded to force alignment on x86
        // to facilitate easy SSE writing with the dedicated functions, keep the next few sets aligned and counted
        __declspec(align(16))// if CSubPicAllocatorPresenterImpl doesn't align well (for a 'Release' build), this line will give a warning during compiling
        double                              m_dVideoWidth;// split here, as m_dVideoHeight should not get 16-byte alignment
        double                              m_dVideoHeight;
        float                               m_fVideoWidth, m_fVideoHeight, m_fVideoWidthr, m_fVideoHeightr;// use for the constant data input in the shader passes, not for math (except for SSE)
        double                              m_dWindowWidth, m_dWindowHeight;
        float                               m_fWindowWidth, m_fWindowHeight, m_fWindowWidthr, m_fWindowHeightr;// use for the constant data input in the shader passes, not for math (except for SSE)
        // section end of aligned requirements

        HANDLE m_hEvtQuit;// stop rendering thread event
        LPCSTR m_pProfile;// points to external
        unsigned __int16 m_u16DXSdkRelease;
        bool m_bVMR9HighColorResolutionCurrent;
        HINSTANCE m_hDWMAPI, m_hD3D9, m_hD3DX9Dll, m_hD3DCompiler;// m_hD3D9 must be freed last, absolutely no D3D resource may remain at that point

        CCritSec m_csAllocatorLock;
        CComPtr<IDirect3D9Ex> m_pD3DEx;
        CComPtr<IDirect3D9> m_pD3D;
        CComPtr<IDirect3DDevice9Ex> m_pD3DDevEx;
        CComPtr<IDirect3DDevice9> m_pD3DDev;

        CComPtr<IDirect3DTexture9> m_apVideoTexture[MAX_PICTURE_SLOTS];
        CComPtr<IDirect3DSurface9> m_apVideoSurface[MAX_PICTURE_SLOTS];
        CComPtr<IDirect3DTexture9> m_pOSDTexture;
        CComPtr<IDirect3DSurface9> m_pOSDSurface;
        CComPtr<ID3DXLine> m_pLine;
        CComPtr<ID3DXFont> m_pFont;
        CComPtr<ID3DXSprite> m_pSprite;
        CSyncRenderer* m_pOuterEVR;

        struct EXTERNALSHADER {IDirect3DPixelShader9* pPixelShader; char* strSrcData; UINT uiSrcLen;};
        CAtlList<EXTERNALSHADER> m_apCustomPixelShaders[2];
        CComPtr<IDirect3DPixelShader9> m_pResizerPixelShaderX, m_pResizerPixelShaderY;
        RECT                                m_rcClearTop, m_rcClearLeft, m_rcClearRight, m_rcClearBottom;// if the resizer render target is full screen or more, no clearing is needed
        bool                                m_bNoXresize, m_bNoYresize;// for activating the resizer section
        size_t                              m_upResizerN;
        size_t m_upDX9ResizerTest;
        RECT m_rcVideoRectTest;
        XForm m_xformTest;
        CComPtr<IDirect3DTexture9> m_pIntermediateResizeTexture;
        CComPtr<IDirect3DSurface9> m_pIntermediateResizeSurface;
        CComPtr<IDirect3DTexture9> m_pScreenSizeTemporaryTexture[2];

        D3DFORMAT m_dfSurfaceType;
        D3DFORMAT m_dfDisplayType;
        D3DCAPS9 m_dcCaps;
        D3DPRESENT_PARAMETERS pp;

        bool SettingsNeedResetDevice();
        void SendResetRequest();
        virtual HRESULT CreateDXDevice(CString& _Error);
        virtual HRESULT ResetDXDevice(CString& _Error);
        virtual HRESULT AllocSurfaces(D3DFORMAT Format = D3DFMT_A8R8G8B8);
        virtual void DeleteSurfaces();
        UINT GetAdapter(IDirect3D9* pD3D);

        LONGLONG m_llLastAdapterCheck;
        UINT m_uiCurrentAdapter;

        HRESULT InitResizers(const int iDX9Resizer);

        // Functions to trace timing performance
        void SyncStats(LONGLONG syncTime);
        void SyncOffsetStats(LONGLONG syncOffset);
        void DrawText(const RECT& rc, const CString& strText, int _Priority);
        void DrawStats();

        MFOffset GetOffset(float v);
        MFVideoArea GetArea(float x, float y, DWORD width, DWORD height);
        bool ClipToSurface(IDirect3DSurface9* pSurface, CRect& s, CRect& d);

        HRESULT DrawRectBase(IDirect3DDevice9* pD3DDev, MYD3DVERTEX<0> v[4]);
        HRESULT DrawRect(DWORD _Color, DWORD _Alpha, const CRect& _Rect);
        HRESULT TextureCopy(IDirect3DTexture9* pTexture);
        HRESULT TextureResize(IDirect3DTexture9* pTexture, const Vector dst[4], const CRect& srcRect);
        HRESULT TextureResize2pass(IDirect3DTexture9* pTexture, const Vector dst[4], const CRect& srcRect);

        __forceinline HRESULT CompileShader(__in char const* szSrcData, __in size_t srcDataLen, __in char const* szFunctionName, __out_opt IDirect3DPixelShader9** ppPixelShader) const;

        // d3dx9_??.dll
        typedef HRESULT(WINAPI* D3DXCreateSpritePtr)(LPDIRECT3DDEVICE9 pDevice, LPD3DXSPRITE* ppSprite); // remove this one, it's useless
        typedef HRESULT(WINAPI* D3DXLoadSurfaceFromMemoryPtr)(__in  LPDIRECT3DSURFACE9 pDestSurface, __in  const PALETTEENTRY* pDestPalette, __in  const RECT* pDestRect, __in  LPCVOID pSrcMemory, __in  D3DFORMAT SrcFormat, __in  UINT SrcPitch, __in  const PALETTEENTRY* pSrcPalette, __in  const RECT* pSrcRect, __in  DWORD Filter, __in  D3DCOLOR ColorKey);
        typedef HRESULT(WINAPI* D3DXLoadSurfaceFromSurfacePtr)(__in LPDIRECT3DSURFACE9 pDestSurface, __in PALETTEENTRY const* pDestPalette, __in RECT const* pDestRect, __in LPDIRECT3DSURFACE9 pSrcSurface, __in PALETTEENTRY const* pSrcPalette, __in RECT const* pSrcRect, __in DWORD Filter, __in D3DCOLOR ColorKey);
        typedef LPCSTR(WINAPI* D3DXGetPixelShaderProfilePtr)(__in LPDIRECT3DDEVICE9 pDevice);
        typedef HRESULT(WINAPI* D3DXCreateLinePtr)(__in LPDIRECT3DDEVICE9 pDevice, __out LPD3DXLINE* ppLine);
        typedef HRESULT(WINAPI* D3DXCreateFontWPtr)(__in LPDIRECT3DDEVICE9 pDevice, __in INT Height, __in UINT Width, __in UINT Weight, __in UINT MipLevels, __in BOOL Italic, __in DWORD CharSet, __in DWORD OutputPrecision, __in DWORD Quality, __in DWORD PitchAndFamily, __in LPCTSTR pFacename, __out LPD3DXFONT* ppFont);
        // D3DCompiler_??.dll
        typedef HRESULT(WINAPI* D3DCompilePtr)(__in_bcount(SrcDataSize) LPCVOID pSrcData, __in SIZE_T SrcDataSize, __in_opt LPCSTR pSourceName, __in_xcount_opt(pDefines->Name != nullptr) CONST D3D_SHADER_MACRO* pDefines, __in_opt ID3DInclude* pInclude, __in LPCSTR pEntrypoint, __in LPCSTR pTarget, __in UINT Flags1, __in UINT Flags2, __out ID3DBlob** ppCode, __out_opt ID3DBlob** ppErrorMsgs);
        // dwmapi.dll
        typedef HRESULT(WINAPI* DwmIsCompositionEnabledPtr)(__out BOOL* pfEnabled);
        typedef HRESULT(WINAPI* DwmEnableCompositionPtr)(__in UINT uCompositionAction);

        // d3dx9_??.dll
        D3DXCreateSpritePtr                 m_pD3DXCreateSprite;// remove this one, it's useless
        D3DXLoadSurfaceFromMemoryPtr        m_pD3DXLoadSurfaceFromMemory;
        D3DXLoadSurfaceFromSurfacePtr       m_pD3DXLoadSurfaceFromSurface;
        D3DXGetPixelShaderProfilePtr        m_pD3DXGetPixelShaderProfile;
        D3DXCreateLinePtr                   m_pD3DXCreateLine;
        D3DXCreateFontWPtr                  m_pD3DXCreateFontW;
        // D3DCompiler_??.dll
        D3DCompilePtr                       m_fnD3DCompile;
        // dwmapi.dll
        DwmIsCompositionEnabledPtr          m_pDwmIsCompositionEnabled;
        DwmEnableCompositionPtr             m_pDwmEnableComposition;

        __declspec(nothrow noalias) void Paint(__in bool fAll);
        HRESULT AlphaBlt(RECT* pSrc, RECT* pDst, IDirect3DTexture9* pTexture);

        virtual void OnResetDevice() {};
        bool m_bPendingResetDevice, m_bDeviceResetRequested;

        ptrdiff_t m_ipTearingPos;
        VMR9AlphaBitmap m_abVMR9AlphaBitmap;
        CAutoVectorPtr<BYTE> m_avVMR9AlphaBitmapData;
        long m_lVMR9AlphaBitmapWidthBytes;

        size_t m_nDXSurface; // Total number of DX Surfaces
        size_t m_nVMR9Surfaces;
        size_t m_iVMR9Surface;
        size_t m_nCurSurface; // Surface currently displayed
        size_t m_nUsedBuffer;

        LONG m_lNextSampleWait; // Waiting time for next sample in EVR
        bool m_bSnapToVSync; // True if framerate is low enough so that snap to vsync makes sense

        UINT m_uScanLineEnteringPaint; // The active scan line when entering Paint()
        REFERENCE_TIME m_llEstVBlankTime; // Next vblank start time in reference clock "coordinates"

        double m_dAverageFrameRate; // Estimate the true FPS as given by the distance between vsyncs when a frame has been presented
        double m_dJitterStdDev; // VSync estimate std dev
        double m_dJitterMean; // Mean time between two syncpulses when a frame has been presented (i.e. when Paint() has been called

        double m_dSyncOffsetAvr; // Mean time between the call of Paint() and vsync. To avoid tearing this should be several ms at least
        double m_dSyncOffsetStdDev; // The std dev of the above

        size_t m_upHighColorResolution;
        size_t m_upDesktopCompositionDisabled;
        size_t m_upIsFullscreen;
        BOOL m_boCompositionEnabled;
        DWORD m_dMainThreadId;

        CSize m_ScreenSize;

        // Display and frame rates and cycles
        double m_dDetectedScanlineTime; // Time for one (horizontal) scan line. Extracted at stream start and used to calculate vsync time
        double m_dD3DRefreshRate; // As set when creating the d3d device
        double m_dD3DRefreshCycle; // Display refresh cycle ms
        double m_dEstRefreshCycle; // As estimated from scan lines
        double m_dFrameCycle; // Average sample time, extracted from the samples themselves
        // double m_fps is defined in ISubPic.h
        double m_dOptimumDisplayCycle; // The display cycle that is closest to the frame rate. A multiple of the actual display cycle
        double m_dCycleDifference; // Difference in video and display cycle time relative to the video cycle time

        UINT m_pcFramesDropped;
        UINT m_pcFramesDuplicated;
        UINT m_pcFramesDrawn;

        LONGLONG m_llJitter[NB_JITTER]; // Vertical sync time stats
        LONGLONG m_llSyncOffset[NB_JITTER]; // Sync offset time stats
        size_t m_upNextJitter;
        size_t m_upNextSyncOffset;

        LONGLONG m_llLastSyncTime;

        LONGLONG m_llMaxJitter;
        LONGLONG m_llMinJitter;
        LONGLONG m_llMaxSyncOffset;
        LONGLONG m_llMinSyncOffset;
        size_t m_upSyncGlitches;

        LONGLONG m_llSampleTime, m_llLastSampleTime; // Present time for the current sample
        long m_lSampleLatency, m_lLastSampleLatency; // Time between intended and actual presentation time
        long m_lMinSampleLatency, m_lLastMinSampleLatency;
        LONGLONG m_llHysteresis;
        long m_lHysteresis;
        long m_lShiftToNearest, m_lShiftToNearestPrev;
        bool m_bVideoSlowerThanDisplay;

        size_t m_upInterlaced;
        double m_TextScale;
        CString  m_strStatsMsg[10];

        CGenlock* m_pGenlock; // The video - display synchronizer class
        CComPtr<IReferenceClock> m_pRefClock; // The reference clock. Used in Paint()
        CComPtr<IAMAudioRendererStats> m_pAudioStats; // Audio statistics from audio renderer. To check so that audio is in sync
        DWORD m_lAudioLag; // Time difference between audio and video when the audio renderer is matching rate to the external reference clock
        long m_lAudioLagMin, m_lAudioLagMax; // The accumulated difference between the audio renderer and the master clock
        DWORD m_lAudioSlaveMode; // To check whether the audio renderer matches rate with SyncClock (returns the value 4 if it does)

        double GetRefreshRate(); // Get the best estimate of the display refresh rate in Hz
        double GetDisplayCycle(); // Get the best estimate of the display cycle time in milliseconds
        double GetCycleDifference(); // Get the difference in video and display cycle times.
        void EstimateRefreshTimings(); // Estimate the times for one scan line and one frame respectively from the actual refresh data

    public:
        CBaseAP(HWND hWnd, HRESULT* phr, CString& _Error);
        ~CBaseAP();

        CCritSec m_csVMR9AlphaBitmapLock;
        void UpdateAlphaBitmap();
        void ResetStats();
        void ResetLocalDevice();

        // ISubPicAllocatorPresenter
        __declspec(nothrow noalias)             HRESULT     GetDIB(__out_opt void* pDib, __inout size_t* pSize);
        __declspec(nothrow noalias)             HRESULT     SetPixelShader(__in CStringW const* pstrSrcData, __in unsigned __int8 u8RenderStage);
    };

    class CSyncAP:
        public CBaseAP,
        public IMFGetService,
        public IMFTopologyServiceLookupClient,
        public IMFVideoDeviceID,
        public IMFVideoPresenter,
        public IDirect3DDeviceManager9,
        public IMFAsyncCallback,
        public IQualProp,
        public IMFRateSupport,
        public IMFVideoDisplayControl,
        public IEVRTrustedVideoPlugin,
        public ISyncClockAdviser
    {
    public:
        CSyncAP(HWND hWnd, HRESULT* phr, CString& _Error);
        ~CSyncAP();

        // IUnknown
        __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv);
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef();
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release();

        // ISubPicAllocatorPresenter
        __declspec(nothrow noalias)             HRESULT     CreateRenderer(__out_opt IBaseFilter** ppRenderer);
        __declspec(nothrow noalias)             void        ResetDevice();

        STDMETHODIMP GetNativeVideoSize(long* lpWidth, long* lpHeight, long* lpARWidth, long* lpARHeight);
        HRESULT InitializeDevice();

        // IMFClockStateSink
        STDMETHODIMP OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset);
        STDMETHODIMP OnClockStop(MFTIME hnsSystemTime);
        STDMETHODIMP OnClockPause(MFTIME hnsSystemTime);
        STDMETHODIMP OnClockRestart(MFTIME hnsSystemTime);
        STDMETHODIMP OnClockSetRate(MFTIME hnsSystemTime, float flRate);

        // IBaseFilter delegate
        bool GetState(DWORD dwMilliSecsTimeout, FILTER_STATE* State, HRESULT& _ReturnValue);

        // IQualProp (EVR statistics window). These are incompletely implemented currently
        STDMETHODIMP get_FramesDroppedInRenderer(int* pcFrames);
        STDMETHODIMP get_FramesDrawn(int* pcFramesDrawn);
        STDMETHODIMP get_AvgFrameRate(int* piAvgFrameRate);
        STDMETHODIMP get_Jitter(int* iJitter);
        STDMETHODIMP get_AvgSyncOffset(int* piAvg);
        STDMETHODIMP get_DevSyncOffset(int* piDev);

        // IMFRateSupport
        STDMETHODIMP GetSlowestRate(MFRATE_DIRECTION eDirection, BOOL fThin, float* pflRate);
        STDMETHODIMP GetFastestRate(MFRATE_DIRECTION eDirection, BOOL fThin, float* pflRate);
        STDMETHODIMP IsRateSupported(BOOL fThin, float flRate, float* pflNearestSupportedRate);
        float GetMaxRate(BOOL bThin);

        // IMFVideoPresenter
        STDMETHODIMP ProcessMessage(MFVP_MESSAGE_TYPE eMessage, ULONG_PTR ulParam);
        STDMETHODIMP GetCurrentMediaType(__deref_out IMFVideoMediaType** ppMediaType);

        // IMFTopologyServiceLookupClient
        STDMETHODIMP InitServicePointers(__in  IMFTopologyServiceLookup* pLookup);
        STDMETHODIMP ReleaseServicePointers();

        // IMFVideoDeviceID
        STDMETHODIMP GetDeviceID(__out  IID* pDeviceID);

        // IMFGetService
        STDMETHODIMP GetService(__RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject);

        // IMFAsyncCallback
        STDMETHODIMP GetParameters(__RPC__out DWORD* pdwFlags, /* [out] */ __RPC__out DWORD* pdwQueue);
        STDMETHODIMP Invoke(__RPC__in_opt IMFAsyncResult* pAsyncResult);

        // IMFVideoDisplayControl
        STDMETHODIMP GetNativeVideoSize(SIZE* pszVideo, SIZE* pszARVideo);
        STDMETHODIMP GetIdealVideoSize(SIZE* pszMin, SIZE* pszMax);
        STDMETHODIMP SetVideoPosition(const MFVideoNormalizedRect* pnrcSource, const LPRECT prcDest);
        STDMETHODIMP GetVideoPosition(MFVideoNormalizedRect* pnrcSource, LPRECT prcDest);
        STDMETHODIMP SetAspectRatioMode(DWORD dwAspectRatioMode);
        STDMETHODIMP GetAspectRatioMode(DWORD* pdwAspectRatioMode);
        STDMETHODIMP SetVideoWindow(HWND hwndVideo);
        STDMETHODIMP GetVideoWindow(HWND* phwndVideo);
        STDMETHODIMP RepaintVideo();
        STDMETHODIMP GetCurrentImage(BITMAPINFOHEADER* pBih, BYTE** pDib, DWORD* pcbDib, LONGLONG* pTimeStamp);
        STDMETHODIMP SetBorderColor(COLORREF Clr);
        STDMETHODIMP GetBorderColor(COLORREF* pClr);
        STDMETHODIMP SetRenderingPrefs(DWORD dwRenderFlags);
        STDMETHODIMP GetRenderingPrefs(DWORD* pdwRenderFlags);
        STDMETHODIMP SetFullscreen(BOOL fFullscreen);
        STDMETHODIMP GetFullscreen(BOOL* pfFullscreen);

        // IEVRTrustedVideoPlugin
        STDMETHODIMP IsInTrustedVideoMode(BOOL* pYes);
        STDMETHODIMP CanConstrict(BOOL* pYes);
        STDMETHODIMP SetConstriction(DWORD dwKPix);
        STDMETHODIMP DisableImageExport(BOOL bDisable);

        // IDirect3DDeviceManager9
        STDMETHODIMP ResetDevice(IDirect3DDevice9* pDevice, UINT resetToken);
        STDMETHODIMP OpenDeviceHandle(HANDLE* phDevice);
        STDMETHODIMP CloseDeviceHandle(HANDLE hDevice);
        STDMETHODIMP TestDevice(HANDLE hDevice);
        STDMETHODIMP LockDevice(HANDLE hDevice, IDirect3DDevice9** ppDevice, BOOL fBlock);
        STDMETHODIMP UnlockDevice(HANDLE hDevice, BOOL fSaveState);
        STDMETHODIMP GetVideoService(HANDLE hDevice, REFIID riid, void** ppService);

    protected:
        void OnResetDevice();
        //        MFCLOCK_STATE m_LastClockState;

    private:
        // dxva.dll
        typedef HRESULT(WINAPI* PTR_DXVA2CreateDirect3DDeviceManager9)(UINT* pResetToken, IDirect3DDeviceManager9** ppDeviceManager);
        // mf.dll
        typedef HRESULT(WINAPI* PTR_MFCreatePresentationClock)(IMFPresentationClock** ppPresentationClock);
        // evr.dll
        typedef HRESULT(WINAPI* PTR_MFCreateDXSurfaceBuffer)(REFIID riid, IUnknown* punkSurface, BOOL fBottomUpWhenLinear, IMFMediaBuffer** ppBuffer);
        typedef HRESULT(WINAPI* PTR_MFCreateVideoSampleFromSurface)(IUnknown* pUnkSurface, IMFSample** ppSample);
        // avrt.dll
        typedef HANDLE(WINAPI* PTR_AvSetMmThreadCharacteristicsW)(LPCWSTR TaskName, LPDWORD TaskIndex);
        typedef BOOL (WINAPI* PTR_AvSetMmThreadPriority)(HANDLE AvrtHandle, AVRT_PRIORITY Priority);
        typedef BOOL (WINAPI* PTR_AvRevertMmThreadCharacteristics)(HANDLE AvrtHandle);

        typedef enum {
            Started = State_Running,
            Stopped = State_Stopped,
            Paused = State_Paused,
            Shutdown = State_Running + 1
        } RENDER_STATE;

        IMFTransform* m_pMixer;
        IMFMediaType* m_pMixerType;
        IMediaEventSink* m_pSink;
        IMFClock* m_pClock;
        IDirect3DDeviceManager9* m_pD3DManager;
        MFVideoRenderPrefs m_dwVideoRenderPrefs;
        COLORREF m_BorderColor;

        HANDLE m_hEvtQuit; // Stop rendering thread event
        HANDLE m_hEvtFlush; // Discard all buffers
        HANDLE m_hEvtSkip; // Skip frame
        bool m_bEvtQuit;
        bool m_bEvtFlush;
        bool m_bEvtSkip;

        bool m_bUseInternalTimer;
        bool m_bPendingRenegotiate;
        bool m_bPendingMediaFinished;
        bool m_bPrerolled; // true if first sample has been displayed.

        HANDLE m_hRenderThread;
        HANDLE m_hMixerThread;
        RENDER_STATE m_nRenderState;
        bool m_bStepping;

        CCritSec m_SampleQueueLock;
        CCritSec m_ImageProcessingLock;

        CInterfaceList<IMFSample, &IID_IMFSample> m_FreeSamples;
        CInterfaceList<IMFSample, &IID_IMFSample> m_ScheduledSamples;
        UINT m_nResetToken;
        int m_nStepCount;

        bool GetSampleFromMixer();
        void MixerThread();
        static DWORD WINAPI MixerThreadStatic(LPVOID lpParam);
        void RenderThread();
        static DWORD WINAPI RenderThreadStatic(LPVOID lpParam);

        void StartWorkerThreads();
        void StopWorkerThreads();
        HRESULT CheckShutdown() const;
        void CompleteFrameStep(bool bCancel);

        void RemoveAllSamples();

        // ISyncClockAdviser
        __declspec(nothrow noalias) void AdviseSyncClock(__inout ISyncClock* sC);

        HRESULT BeginStreaming();
        HRESULT GetFreeSample(IMFSample** ppSample);
        HRESULT GetScheduledSample(IMFSample** ppSample, int& _Count);
        void MoveToFreeList(IMFSample* pSample, bool bTail);
        void MoveToScheduledList(IMFSample* pSample, bool _bSorted);
        void FlushSamples();
        void FlushSamplesInternal();

        __declspec(nothrow noalias) __int8 GetMediaTypeMerit(IMFMediaType* pMixerType);
        HRESULT RenegotiateMediaType();

        // Functions pointers for Vista/.NET3 specific library
        PTR_DXVA2CreateDirect3DDeviceManager9 pfDXVA2CreateDirect3DDeviceManager9;
        PTR_MFCreateDXSurfaceBuffer pfMFCreateDXSurfaceBuffer;
        PTR_MFCreateVideoSampleFromSurface pfMFCreateVideoSampleFromSurface;

        PTR_AvSetMmThreadCharacteristicsW pfAvSetMmThreadCharacteristicsW;
        PTR_AvSetMmThreadPriority pfAvSetMmThreadPriority;
        PTR_AvRevertMmThreadCharacteristics pfAvRevertMmThreadCharacteristics;
    };

    class CSyncRenderer:
        public CUnknown,
        public IVMRffdshow9,// uses default function declared for IVMRffdshow9
        public IVMRMixerBitmap9,
        public IBaseFilter
    {
        CComPtr<IUnknown> m_pEVR;
        VMR9AlphaBitmap* m_pVMR9AlphaBitmap;
        CSyncAP* m_pAllocatorPresenter;

    public:
        CSyncRenderer(const TCHAR* pName, LPUNKNOWN pUnk, HRESULT& hr, VMR9AlphaBitmap* pVMR9AlphaBitmap, CSyncAP* pAllocatorPresenter);
        ~CSyncRenderer();

        // IBaseFilter
        virtual HRESULT STDMETHODCALLTYPE EnumPins(__out IEnumPins** ppEnum);
        virtual HRESULT STDMETHODCALLTYPE FindPin(LPCWSTR Id, __out IPin** ppPin);
        virtual HRESULT STDMETHODCALLTYPE QueryFilterInfo(__out FILTER_INFO* pInfo);
        virtual HRESULT STDMETHODCALLTYPE JoinFilterGraph(__in_opt IFilterGraph* pGraph, __in_opt LPCWSTR pName);
        virtual HRESULT STDMETHODCALLTYPE QueryVendorInfo(__out LPWSTR* pVendorInfo);
        virtual HRESULT STDMETHODCALLTYPE Stop();
        virtual HRESULT STDMETHODCALLTYPE Pause();
        virtual HRESULT STDMETHODCALLTYPE Run(REFERENCE_TIME tStart);
        virtual HRESULT STDMETHODCALLTYPE GetState(DWORD dwMilliSecsTimeout, __out FILTER_STATE* State);
        virtual HRESULT STDMETHODCALLTYPE SetSyncSource(__in_opt  IReferenceClock* pClock);
        virtual HRESULT STDMETHODCALLTYPE GetSyncSource(__deref_out_opt  IReferenceClock** pClock);
        virtual HRESULT STDMETHODCALLTYPE GetClassID(__RPC__out CLSID* pClassID);

        // IVMRMixerBitmap9
        STDMETHODIMP GetAlphaBitmapParameters(VMR9AlphaBitmap* pBmpParms);
        STDMETHODIMP SetAlphaBitmap(const VMR9AlphaBitmap*  pBmpParms);
        STDMETHODIMP UpdateAlphaBitmapParameters(const VMR9AlphaBitmap* pBmpParms);

        // IUnknown
        __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv) {
            return GetOwner()->QueryInterface(riid, ppv);
        }
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef() {
            return GetOwner()->AddRef();
        }
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release() {
            return GetOwner()->Release();
        }
        __declspec(nothrow noalias) STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void** ppv) {
            ASSERT(ppv);

            __assume(this);// fix assembly: the compiler generated tests for null pointer input on static_cast<T>(this)
            ASSERT(m_pEVR);
            HRESULT hr;
            if (riid == IID_IVMRMixerBitmap9) { *ppv = static_cast<IVMRMixerBitmap9*>(this); }
            else if (riid == IID_IVMRffdshow9) { *ppv = static_cast<IVMRffdshow9*>(this); }
            else if (riid == IID_IBaseFilter) { *ppv = static_cast<IBaseFilter*>(this); }
            else if (riid == IID_IMediaFilter) { *ppv = static_cast<IMediaFilter*>(this); }
            else if (riid == IID_IPersist) { *ppv = static_cast<IPersist*>(this); }
            else if (SUCCEEDED(hr = m_pEVR->QueryInterface(riid, ppv))) { return hr; }
            else { return __super::NonDelegatingQueryInterface(riid, ppv); }
            GetOwner()->AddRef();
            return NOERROR;
        }
    };

#define GENLOCK_MAX_FIFO_SIZE 1024
    class CGenlock
    {
    public:
        class MovingAverage
        {
        public:
            MovingAverage(int size):
                fifoSize(size),
                oldestSample(0),
                sum(0) {
                if (fifoSize > GENLOCK_MAX_FIFO_SIZE) {
                    fifoSize = GENLOCK_MAX_FIFO_SIZE;
                }
                for (int i = 0; i < GENLOCK_MAX_FIFO_SIZE; i++) {
                    fifo[i] = 0;
                }
            }

            ~MovingAverage() {
            }

            double Average(double sample) {
                sum = sum + sample - fifo[oldestSample];
                fifo[oldestSample] = sample;
                oldestSample++;
                if (oldestSample == fifoSize) {
                    oldestSample = 0;
                }
                return sum / fifoSize;
            }

        private:
            double fifo[GENLOCK_MAX_FIFO_SIZE];
            double sum;
            int fifoSize;
            int oldestSample;
        };

        CGenlock(CRenderersSettings const* pkRendererSettings, double target, double limit, int rowD, int colD, double clockD, UINT mon);
        ~CGenlock();

        BOOL PowerstripRunning(); // TRUE if PowerStrip is running
        HRESULT GetTiming(); // Get the string representing the display's current timing parameters
        HRESULT ResetTiming(); // Reset timing to what was last registered by GetTiming()
        HRESULT ResetClock(); // Reset reference clock speed to nominal
        HRESULT SetTargetSyncOffset(double targetD);
        HRESULT GetTargetSyncOffset(double* targetD);
        HRESULT SetControlLimit(double cL);
        HRESULT GetControlLimit(double* cL);
        HRESULT SetDisplayResolution(UINT columns, UINT lines);

        // ISyncClockAdviser
        __declspec(nothrow noalias) void AdviseSyncClock(__inout ISyncClock* sC);

        HRESULT SetMonitor(UINT mon); // Set the number of the monitor to synchronize
        HRESULT ResetStats(); // Reset timing statistics

        HRESULT ControlDisplay(double syncOffset, double frameCycle); // Adjust the frequency of the display if needed
        HRESULT ControlClock(double syncOffset, double frameCycle); // Adjust the frequency of the clock if needed
        HRESULT UpdateStats(double syncOffset, double frameCycle); // Don't adjust anything, just update the syncOffset stats

        BOOL powerstripTimingExists; // TRUE if display timing has been got through Powerstrip
        BOOL liveSource; // TRUE if live source -> display sync is the only option
        int adjDelta; // -1 for display slower in relation to video, 0 for keep, 1 for faster
        int lineDelta; // The number of rows added or subtracted when adjusting display fps
        int columnDelta; // The number of colums added or subtracted when adjusting display fps
        double cycleDelta; // Adjustment factor for cycle time as fraction of nominal value
        UINT displayAdjustmentsMade; // The number of adjustments made to display refresh rate
        UINT clockAdjustmentsMade; // The number of adjustments made to clock frequency

        UINT totalLines, totalColumns; // Including the porches and sync widths
        UINT visibleLines, visibleColumns; // The nominal resolution
        CRenderersSettings const* const mk_pRendererSettings;
        MovingAverage* syncOffsetFifo;
        MovingAverage* frameCycleFifo;
        double minSyncOffset, maxSyncOffset;
        double syncOffsetAvg; // Average of the above
        double minFrameCycle, maxFrameCycle;
        double frameCycleAvg;

        UINT pixelClock; // In pixels/s
        double displayFreqCruise;  // Nominal display frequency in frames/s
        double displayFreqSlower;
        double displayFreqFaster;
        double curDisplayFreq; // Current (adjusted) display frequency
        double controlLimit; // How much the sync offset is allowed to drift from target sync offset
        WPARAM monitor; // The monitor to be controlled. 0-based.
        CComPtr<ISyncClock> syncClock; // Interface to an adjustable reference clock

    private:
        HWND psWnd; // PowerStrip window
        const static int TIMING_PARAM_CNT = 10;
        const static int MAX_LOADSTRING = 100;
        UINT displayTiming[TIMING_PARAM_CNT]; // Display timing parameters
        UINT displayTimingSave[TIMING_PARAM_CNT]; // So that we can reset the display at exit
        TCHAR faster[MAX_LOADSTRING]; // String corresponding to faster display frequency
        TCHAR cruise[MAX_LOADSTRING]; // String corresponding to nominal display frequency
        TCHAR slower[MAX_LOADSTRING]; // String corresponding to slower display frequency
        TCHAR savedTiming[MAX_LOADSTRING]; // String version of saved timing (to be restored upon exit)
        double lowSyncOffset; // The closest we want to let the scheduled render time to get to the next vsync. In % of the frame time
        double targetSyncOffset; // Where we want the scheduled render time to be in relation to the next vsync
        double highSyncOffset; // The furthers we want to let the scheduled render time to get to the next vsync
        CCritSec csGenlockLock;
    };
}

__declspec(nothrow noalias) HRESULT CreateEVRS(HWND hWnd, GothSync::CSyncAP** ppAP);
