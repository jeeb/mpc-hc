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

// only for debugging
//#define DISABLE_USING_D3D9EX

#include "AllocatorCommon.h"
#include "RenderersSettings.h"
#include "../../../SubPic/DX9SubPic.h"
#include "UtilityWindow.h"
#include <vmr9.h>
#include <dxva2api.h>
#include <dwmapi.h>
#include <LowLevelMonitorConfigurationAPI.h>

#define NB_JITTER                   256// keep this one at a power of 2, minimum 16
#define MAX_VIDEO_SURFACES          16// EVR CP's queue handler has a maximum of 16, this is also limited by the amount that can be set in the renderers settings for mixer buffers

#define PCIV_AMD                    0x1022
#define PCIV_ATI                    0x1002
#define PCIV_INTEL                  0x8086

// Defined in MainFrm.cpp
extern bool g_bNoDuration;
extern bool g_bExternalSubtitleTime;

namespace DSObjects
{
    extern __declspec(nothrow noalias noinline noreturn) void ErrBox(__in HRESULT hr, __in wchar_t const* szText);
    extern __declspec(nothrow noalias noinline) unsigned __int8 GetChromaType(__in unsigned __int32 u32SubType);

    class __declspec(novtable) CDX9AllocatorPresenter
        : public CSubPicAllocatorPresenterImpl
        , public IVMRMixerBitmap9
        , public IVMRffdshow9// uses default function declared for IVMRffdshow9
        , public CUtilityWindow
    {
    protected:
        __declspec(nothrow noalias) ~CDX9AllocatorPresenter();// polymorphic class not implementing IUnknown, so a non-virtual destructor
    private:
        // padding to 16 byte alignment
        HINSTANCE                               m_hD3DX9Dll, m_hD3D9;// m_hD3D9 must be freed last, absolutely no D3D resource may remain at that point
        double                                  m_dMonitorHeight;

        // section begin of 16-byte aligned requirements, this is tested in the constructor function at compile time
        float                                   m_afStatsBarsJGraph[100];// this avoids D3DXVECTOR2's useless default initializers, and avoids the struct, so that the compiler can do a better job
        wchar_t                                 m_awcD3D9Device[MAX_DEVICE_IDENTIFIER_STRING];// inherited from D3DADAPTER_IDENTIFIER9
        char                                    m_szGDIDisplayDeviceName[32];
        char                                    m_szXYZtoRGBmatrix[128];

        // 12 bytes padding, the larger one first for alignment
        // CreateDevice() initializes these
    protected:
        double                                  m_dPrevStartPaint;
        __int32                                 m_i32MonitorHeight;

    private:
        // non-zero initialized values for VSync functions
        __int32                                 m_i32VBlankMin;// initialized as a set of six values
        __int32                                 m_i32VBlankEndPresent;
        unsigned __int32                        m_u32InitialVSyncWait;
        double                                  m_dPaintTimeMin;
        double                                  m_dRasterStatusWaitTimeMin;
    protected:
        double                                  m_dModeratedTimeSpeed;

        // zero initialized values for VSync functions, total number of bytes needs to be modulo 16, this is tested inside the function that needs it
        // set 1:
        bool                                    m_bSyncStatsAvailable;// warning: used as begin value in an element count test
        bool                                    m_bFullscreenToolBarDetected;
        unsigned __int8                         m_u8SchedulerPos;// for the alternative scheduler
        __int8                                  m_i8_filler;// 1 byte of filler
        __int32                                 m_i32VBlankStartMeasure;
        size_t                                  m_upDetectedRefreshRatePos;
        size_t                                  m_upClockTimeChangeHistoryPos;
        size_t                                  m_upNextSyncOffsetPos;
    private:
        size_t                                  m_upDetectedFrameTimePos;
    protected:
        bool                                    m_bDetectedLock;
        unsigned __int8                         m_u8SchedulerAdjustTimeOut;// for the alternative scheduler and frame interpolator, a counter that prevents the detected refresh rate changes too fast after seeking, paused mode and resets
    private:
        bool                                    m_bExFrameSchedulerActive;
        unsigned __int8                         m_u8ForceNextAmount;// for the alternative scheduler, actually doesn't really have to be erased or initialized
        __int32                                 m_i32VBlankEndWait;
        __int32                                 m_i32VBlankStartWait;
        __int32                                 m_i32VBlankMax;
        // set 2:
        RECT                                    m_rcTearing;// tearing test lines
        double                                  m_dRasterStatusWaitTimeMax;
        double                                  m_dRasterStatusWaitTimeMaxCalc;
        double                                  m_dWaitForGPUTime;
        unsigned __int8                         m_au8PresentCountLog[NB_JITTER];
        float                                   m_afFTdividerILog[NB_JITTER];
        double                                  m_adDetectedFrameTimeHistory[NB_JITTER];
        double                                  m_adDetectedFrameTimeRecHistory[NB_JITTER];
        double                                  m_adDetectedRefreshRateList[128];
        double                                  m_adDetectedScanlineRateList[128];
        double                                  m_dDetectedRefreshTimePrim;
        double                                  m_dDetectedScanlineTimePrim;
        double                                  m_dPrevVBlankTime;// to keep track of jitter
    protected:
        double                                  m_dVBlankStartMeasureTime;
        double                                  m_dClockDiffCalc;
        double                                  m_dClockDiffPrim;
        double                                  m_dAverageFrameRate;// Estimate the real FPS
        double                                  m_dModeratedTimeSpeedPrim;
        double                                  m_dDetectedFrameTimeStdDev;
        double                                  m_dDetectedScanlineTime;
        double                                  m_adTimeChangeHistory[128], m_adClockChangeHistory[128];// used as a set of two arrays
        double                                  m_adQueueDepthHistory[32];
        double                                  m_dCurrentQueueDepth;
        double                                  m_dMaxQueueDepth;// warning: used as end value in an element count test

        static_assert(sizeof(RECT) == 16, "struct RECT or platform settings changed");// the struct is declared on a global level
        // RECT structures should keep 16-byte alignment
        RECT                                    m_rcClearLeft, m_rcClearTop, m_rcClearRight, m_rcClearBottom;// if the resizer render target is full screen or more, no clearing is needed
        RECT                                    m_rcResizerStretchRectSrc, m_rcResizerStretchRectDst;// used for StrechRect() resizing
        RECT                                    m_rcSubSrc, m_rcSubDst, m_rcSubDstTest;// metrics for the subtitle renderer, m_rcSubSrc and m_rcSubDst have to be paired together like this

        // the class constructor zero-initializes these values, total number of bytes needs to be modulo 16, this is tested inside the function that needs it
    private:
        double                                  m_dReferenceRefreshRate;// warning: used as begin value in an element count test
        double                                  m_dPrevSettingsCheck;// to check settings once in a while
    protected:
        unsigned __int8                         m_u8ChromaType;// stores chroma sub-sampling, the type is set in the lower bits, type 4:4:4 is 0, 4:2:2 is 1 and 4:2:0 is 2, the high bit is used to indicate that the Y'CbCr input is not horizontally chroma cosited (MPEG1-type), keep this synchronized with m_szChromaCositing
        bool                                    m_bCorrectedFrameTime;
        bool                                    m_bInterlaced;
        unsigned __int8                         m_u8VSyncMode;
        unsigned __int8                         m_u8CurrentMixerSurface;
        unsigned __int8                         m_u8MixerSurfacesUsed;
        static_assert(sizeof(ATOM) == 2, "type ATOM or platform settings changed, reorder CDX9AllocatorPresenter");
        ATOM                                    m_u16RegisteredWindowClassAtom;
        double                                  m_dLastFrameDuration;
        double                                  m_adJitter[NB_JITTER];
        double                                  m_adSyncOffset[NB_JITTER];
        double                                  m_adPaintTimeO[NB_JITTER];
        double                                  m_dJitterMean;
        double                                  m_dPaintTimeMean;
        double                                  m_dJitterStdDev;// Estimate the Jitter std dev
        double                                  m_dSyncOffsetStdDev;
        double                                  m_dSyncOffsetAvr;
        double                                  m_dPaintTime;
    private:
        double                                  m_dPaintTimeMax;
    protected:
        double                                  m_dDetectedScanlinesPerFrame;
        double                                  m_dStreamReferenceVideoFrameRate;
        double                                  m_dDetectedVideoTimePerFrame;

        IDirect3DDevice9Ex*                     m_pD3DDev;// Ex parts are only available on Vista and newer, this class has IDirect3DDevice9 at its base
    private:
        IDirect3D9Ex*                           m_pD3D;// Ex parts are only available on Vista and newer, this class has IDirect3D9 at its base
        HINSTANCE                               m_hD3DCompiler, m_hDWMAPI;
        HANDLE                                  m_hVSyncThread, m_hEvtQuitVSync;
        size_t                                  m_upSchedulerCorrectionCounter;
        size_t                                  m_upLenstrD3D9Device;
        // sub-total: 12 pointer-sized items + 96+3*8*NB_JITTER bytes

        // assorted set of IUnknown pointers: 60+2*MAX_VIDEO_SURFACES
        ID3DXLine*                              m_pLine;// from external m_hD3DX9Dll, warning: used as begin value in an element count test
        ID3DXFont*                              m_pFont;// from external m_hD3DX9Dll

        // utility pointers
        IDirect3DTexture9*                      m_pUtilTexture;
        IDirect3DVolumeTexture9*                m_pUtil3DTexture;
        ID3DBlob*                               m_pUtilD3DBlob;
    protected:
        IDirect3DQuery9*                        m_pUtilEventQuery;

    private:
        // note: the IDirect3DSwapChain9Ex functions are only available when creating an IDirect3DDevice9Ex device with the D3DCREATE_ENABLE_PRESENTSTATS flag
        IDirect3DSwapChain9Ex*                  m_pSwapChain;// Ex parts are only available on Vista and newer, this class has IDirect3DSwapChain9 at its base
        IDirect3DSurface9*                      m_pBackBuffer;
        IDirect3DIndexBuffer9*                  m_pIndexBuffer;
        IDirect3DVertexBuffer9*                 m_pStatsRectVBuffer, *m_pSubBlendVBuffer, *m_pVBuffer;
        IDirect3DPixelShader9*                  m_pPreResizerHorizontalPixelShader, *m_pPreResizerVerticalPixelShader;
        IDirect3DPixelShader9*                  m_pResizerPixelShaderX, *m_pResizerPixelShaderY;
        IDirect3DSurface9*                      m_pIntermediateResizeSurface;
        IDirect3DTexture9*                      m_pIntermediateResizeTexture;

    protected:
        IDirect3DSurface9*                      m_apVideoSurface[MAX_VIDEO_SURFACES];
    private:
        IDirect3DTexture9*                      m_apVideoTexture[MAX_VIDEO_SURFACES];

        // Custom window pixel shaders, Frame interpolator
        IDirect3DSurface9*                      m_apTempVideoSurface[2];
        IDirect3DTexture9*                      m_apTempVideoTexture[2];
        IDirect3DSurface9*                      m_apTempWindowSurface[5];
        IDirect3DTexture9*                      m_apTempWindowTexture[5];

        // Frame interpolator
        IDirect3DPixelShader9*                  m_apFIPixelShader[4];
        IDirect3DSurface9*                      m_pFIBufferRT0, *m_pFIBufferRT1, *m_pFIBufferRT2;// extras used for multiple render target techniques
        IDirect3DPixelShader9*                  m_apFIPrePixelShader[3];
        IDirect3DSurface9*                      m_apFIPreSurface[4];
        IDirect3DTexture9*                      m_apFIPreTexture[4];

        IDirect3DPixelShader9*                  m_pFinalPixelShader, *m_pSubtitlePassPixelShader, *m_pOSDPassPixelShader, *m_pIniatialPixelShader0, *m_pIniatialPixelShader1, *m_pIniatialPixelShader2;
        IDirect3DTexture9*                      m_pDitherTexture;
        IDirect3DVolumeTexture9*                m_pLut3DTexture;
        IDirect3DTexture9*                      m_pOSDTexture;
        IDirect3DTexture9*                      m_pSubtitleTexture;// warning: twice used as end value in an element count test
        // end of assorted set of IUnknown pointers

    protected:
        // sorted floats for the pixel shader passes
        // video pass shaders
        float m_fVideoWidth, m_fVideoHeight, m_fPixelShaderCounter, m_fShaderClock,
              m_fVideoWidthr, m_fVideoHeightr;
        __int64 const mk_i64MustBeZero;// pads the previous set
        // window pass shaders
        float m_fWindowWidth, m_fWindowHeight, m_fPixelShaderCounterCopy, m_fShaderClockCopy,
              m_fWindowWidthr, m_fWindowHeightr, m_fWindowOnMonitorPosLeft, m_fWindowOnMonitorPosTop,
              m_afNormRectVideoArea[4];
        // general metrics (of the parts not covered in the previous sets)
        double                                  m_dVideoWidth, m_dVideoHeight;
        double                                  m_dWindowWidth, m_dWindowHeight;
        __declspec(align(8)) char               m_szVideoWidth[8], m_szVideoHeight[8];
        __declspec(align(8)) char               m_szWindowWidth[8], m_szWindowHeight[8];
        __declspec(align(8)) char               m_szResizedWidth[8], m_szResizedHeight[8];
    private:
        wchar_t                                 m_szMonitorName[14];// used in the stats screen, availability for this name, m_u16MonitorHorRes, m_u16MonitorVerRes, m_u16mmMonitorWidth and m_u16mmMonitorHeight is tested by its first character, the longest name the EDID supports is 13 characters (the zero end is written in the initializer)
        UINT                                    m_uiBaseRefreshRate;

    protected:
        // the present parameters struct is re-used often and contains some of the current settings data
        D3DPRESENT_PARAMETERS                   m_dpPParam;// only needs 8-byte alignment, the fist two values are loaded for SSE work
        // section end of aligned requirements

        D3DCAPS9                                m_dcCaps;
    private:
        VMR9AlphaBitmap                         m_abVMR9AlphaBitmap;// IVMRMixerBitmap9

    protected:
        CStringW                                m_strMixerStatus;
        CStringW                                m_strDecoder;
    public:
        struct EXTERNALSHADER {IDirect3DPixelShader9* pPixelShader; ID3DBlob* pSrcData; char* strSrcData; unsigned __int32 u32SrcLen;};
    private:
        CAtlList<EXTERNALSHADER>                m_apCustomPixelShaders[2];

        RECT                                    m_rcVideoRectTest;

        double                                  m_dMaxJitter;
        double                                  m_dMinJitter;
        double                                  m_dVBlankWaitTime;
        double                                  m_dVBlankLockTime;
        double                                  m_dRasterStatusWaitTime;
    protected:
        double                                  m_dMaxSyncOffset;
        double                                  m_dMinSyncOffset;
        double                                  m_dDetectedRefreshRate, m_dDetectedRefreshTime;
        double                                  m_dRoundedStreamReferenceVideoFrameRate;

        unsigned __int64                        m_u64PerfFreq;
        __int64                                 m_i64PerfCntInit;
        LARGE_INTEGER                           m_liLastPerfCnt;// general usage
        double                                  m_dPerfFreqr;

    private:
        XForm                                   m_xformTest;// really, time to set up something better in mplayerc.cpp, m_xform is much less effective, precise and efficient than a basic set of "flip horizontal", "flip vertical" and "rotate 90 degrees"

        // renderer settings trackers
        unsigned __int32                        m_u32VMR9ColorManagementLookupQualityCurrent;
        unsigned __int8                         m_u8VMR9ColorManagementAmbientLightCurrent;
        unsigned __int8                         m_u8VMR9ColorManagementIntentCurrent;
        unsigned __int8                         m_u8VMR9ColorManagementWpAdaptStateCurrent;
        unsigned __int8                         m_u8VMR9ColorManagementEnableCurrent;
        unsigned __int8                         m_u8VMR9ColorManagementBPCCurrent;
        unsigned __int8                         m_u8VMR9DitheringLevelsCurrent;
        unsigned __int8                         m_u8VMR9DitheringTestEnableCurrent;
        unsigned __int8                         m_u8VMR9SurfacesQuality;
        unsigned __int8                         m_u8VMR9FrameInterpolationCurrent;
        bool                                    m_bVMR9DisableInitialColorMixingCurrent;
        bool                                    m_bHighColorResolutionCurrent;// only the setting tracker
        bool                                    m_bVMR9AlterativeVSyncCurrent;// only the setting tracker
    protected:
        unsigned __int8                         m_u8VMR9ChromaFixCurrent;// available to the mixers to indicate switching between HD and SD Y'CbCr conversion matrices

        // 1-byte aligned
        bool                                    m_bAlternativeVSync;// indicates actual activation
        bool                                    m_bPartialExDeviceReset;// note: partial resets on the IDirect3DDevice9Ex device are available for Vista and newer only
        unsigned __int8                         m_u8MixerSurfaceCount;
    private:
        unsigned __int8                         m_u8DX9ResizerTest;
        unsigned __int8                         m_u8ActiveResizer;
        unsigned __int8                         m_u8ChromaTypeTest;// stores chroma sub-sampling type
        unsigned __int8                         m_u8FIold, m_u8FIprevious, m_u8FInext;// Frame interpolator, swappable pointers
        // 2-byte aligned
        unsigned __int16                        m_u16MonitorHorRes, m_u16MonitorVerRes, m_u16mmMonitorWidth, m_u16mmMonitorHeight;
        // 4-byte aligned
        UINT                                    m_uiCurrentAdapter;
    protected:
        BOOL                                    m_boCompositionEnabled;
        D3DFORMAT                               m_dfSurfaceType;// also used for activating the final pass section and linearization on non-8-bit surfaces
        union {
            struct {
                unsigned __int8                 m_u8OSVersionMinor;
                unsigned __int8                 m_u8OSVersionMajor;
            };
            unsigned __int16                    m_u16OSVersionMinorMajor;
        };
        unsigned __int16                        m_u16OSVersionBuild;

    private:
        __declspec(align(4)) char               m_szLut3DSize[5];
        bool                                    m_bDesktopCompositionDisabled;
        __declspec(align(2)) char               m_szDitheringLevel[5];
        bool const                              mk_bIsEVR;
        __declspec(align(2)) char               m_szColorManagementLevel[2], m_szDitheringTest[2], m_szFrameInterpolationLevel[2];
    protected:
        __declspec(align(2)) char               m_szChromaCositing[2];
    private:
        bool                                    m_bNoHresize, m_bNoVresize;// for activating the resizer section

        // pointer-size aligned, keep this alignment up to the end of the class
        LPCSTR                                  m_pProfile;// points to external
        D3D_SHADER_MACRO                        m_aShaderMacros[18];// see class initializer for the list, along with InternalDX9Shaders.cpp
        CRenderersData* const                   m_pRenderersData;

        __declspec(nothrow noalias) void CreateDevice(__inout CStringW* pstrError);
        __declspec(nothrow noalias) __int64 GetVBlank(__in unsigned __int8 u8VBFlags);// out: BOOL in Vblank low, __int32 scanline number high
        __declspec(nothrow noalias) __int64 WaitForVBlankRange(__in __int32 i32RasterStart, __in __int32 i32RasterSize, __in unsigned __int8 u8VBFlags);// out: unsigned __int8 VBlank flags low, __int32 scanline number at end high
    protected:
        CCritSec                                m_csRefreshRateLock;
        CCritSec                                m_csRenderLock;
        HMONITOR                                m_hCurrentMonitor;
        HINSTANCE                               m_hDXVA2Lib;// CEVRAllocatorPresenter loads some functions from this dll as well

        __declspec(nothrow noalias noinline) void ResetMainDevice();
        __declspec(nothrow noalias noinline) void AllocSurfaces();
        __declspec(nothrow noalias noinline) void DeleteSurfaces();

#define FRENDERPAINT_NORMAL 1
#define FRENDERPAINT_INIT 2
        __declspec(nothrow noalias noinline) void Paint(__in unsigned __int8 u8RPFlags);

    public:
        __declspec(nothrow noalias) CDX9AllocatorPresenter(__in HWND hWnd, __inout CStringW* pstrError, __in bool bIsEVR);

        // CSubPicAllocatorPresenterImpl
        __declspec(nothrow noalias) HRESULT GetDIB(__out_opt void* pDib, __inout size_t* pSize);
        __declspec(nothrow noalias) uintptr_t SetPixelShaders(__in_ecount(2) CAtlList<Shader const*> const aList[2]);
        __declspec(nothrow noalias) void ClearPixelShaders(unsigned __int8 u8RenderStages);

        // IVMRMixerBitmap9
        __declspec(nothrow noalias) STDMETHODIMP GetAlphaBitmapParameters(VMR9AlphaBitmap* pBmpParms);
        __declspec(nothrow noalias) STDMETHODIMP SetAlphaBitmap(VMR9AlphaBitmap const* pBmpParms);
        __declspec(nothrow noalias) STDMETHODIMP UpdateAlphaBitmapParameters(VMR9AlphaBitmap const* pBmpParms) { return SetAlphaBitmap(pBmpParms); }

        // d3dx9_??.dll
        typedef HRESULT(WINAPI* D3DXLoadSurfaceFromSurfacePtr)(__in LPDIRECT3DSURFACE9 pDestSurface, __in PALETTEENTRY const* pDestPalette, __in RECT const* pDestRect, __in LPDIRECT3DSURFACE9 pSrcSurface, __in PALETTEENTRY const* pSrcPalette, __in RECT const* pSrcRect, __in DWORD Filter, __in D3DCOLOR ColorKey);
        typedef LPCSTR(WINAPI* D3DXGetPixelShaderProfilePtr)(__in LPDIRECT3DDEVICE9 pDevice);
        typedef HRESULT(WINAPI* D3DXCreateLinePtr)(__in LPDIRECT3DDEVICE9 pDevice, __out LPD3DXLINE* ppLine);
        typedef HRESULT(WINAPI* D3DXCreateFontWPtr)(__in LPDIRECT3DDEVICE9 pDevice, __in INT Height, __in UINT Width, __in UINT Weight, __in UINT MipLevels, __in BOOL Italic, __in DWORD CharSet, __in DWORD OutputPrecision, __in DWORD Quality, __in DWORD PitchAndFamily, __in LPCTSTR pFacename, __out LPD3DXFONT* ppFont);
        // D3DCompiler_??.dll
        typedef HRESULT(WINAPI* D3DCompilePtr)(__in_bcount(SrcDataSize) LPCVOID pSrcData, __in SIZE_T SrcDataSize, __in_opt LPCSTR pSourceName, __in_xcount_opt(pDefines->Name != nullptr) CONST D3D_SHADER_MACRO* pDefines, __in_opt ID3DInclude* pInclude, __in LPCSTR pEntrypoint, __in LPCSTR pTarget, __in UINT Flags1, __in UINT Flags2, __out ID3DBlob** ppCode, __out_opt ID3DBlob** ppErrorMsgs);
        // dxva2.dll
        typedef HRESULT(WINAPI* GetNumberOfPhysicalMonitorsFromHMONITORPtr)(__in HMONITOR hMonitor, __out LPDWORD pdwNumberOfPhysicalMonitors);
        typedef HRESULT(WINAPI* GetPhysicalMonitorsFromHMONITORPtr)(__in HMONITOR hMonitor, __in DWORD dwPhysicalMonitorArraySize, __out_ecount(dwPhysicalMonitorArraySize) LPPHYSICAL_MONITOR pPhysicalMonitorArray);
        typedef _BOOL(WINAPI* DestroyPhysicalMonitorsPtr)(__in DWORD dwPhysicalMonitorArraySize, __in_ecount(dwPhysicalMonitorArraySize) LPPHYSICAL_MONITOR pPhysicalMonitorArray);
        typedef _BOOL(WINAPI* GetTimingReportPtr)(HANDLE hMonitor, __out LPMC_TIMING_REPORT pmtrMonitorTimingReport);
        typedef HRESULT(WINAPI* DXVA2CreateDirect3DDeviceManager9Ptr)(__out UINT* pResetToken, __out IDirect3DDeviceManager9** ppDXVAManager);
        // typedef HRESULT(WINAPI* DXVA2CreateVideoServicePtr)(IDirect3DDevice9* pDD, REFIID riid, void** ppService);
        // dwmapi.dll
        typedef HRESULT(WINAPI* DwmEnableMMCSSPtr)(BOOL fEnableMMCSS);
        typedef HRESULT(WINAPI* DwmSetDxFrameDurationPtr)(HWND hwnd, INT cRefreshes);
        typedef HRESULT(WINAPI* DwmSetPresentParametersPtr)(HWND hwnd, __inout DWM_PRESENT_PARAMETERS* pPresentParams);
        typedef HRESULT(WINAPI* DwmGetCompositionTimingInfoPtr)(HWND hwnd, __out DWM_TIMING_INFO* pTimingInfo);
        typedef HRESULT(WINAPI* DwmIsCompositionEnabledPtr)(__out BOOL* pfEnabled);
        typedef HRESULT(WINAPI* DwmEnableCompositionPtr)(UINT uCompositionAction);

    private:
        // d3dx9_??.dll
        D3DXLoadSurfaceFromSurfacePtr               m_fnD3DXLoadSurfaceFromSurface;
        D3DXGetPixelShaderProfilePtr                m_fnD3DXGetPixelShaderProfile;
        D3DXCreateLinePtr                           m_fnD3DXCreateLine;
        D3DXCreateFontWPtr                          m_fnD3DXCreateFontW;
        // D3DCompiler_??.dll
        D3DCompilePtr                               m_fnD3DCompile;
        // dxva2.dll
        GetNumberOfPhysicalMonitorsFromHMONITORPtr  m_fnGetNumberOfPhysicalMonitorsFromHMONITOR;
        GetPhysicalMonitorsFromHMONITORPtr          m_fnGetPhysicalMonitorsFromHMONITOR;
        DestroyPhysicalMonitorsPtr                  m_fnDestroyPhysicalMonitors;
        GetTimingReportPtr                          m_fnGetTimingReport;
    protected:
        DXVA2CreateDirect3DDeviceManager9Ptr        m_fnDXVA2CreateDirect3DDeviceManager9;// the DXVA2 device manager is only used by EVR
        // DXVA2CreateVideoServicePtr m_fnDXVA2CreateVideoService;
    private:
        // dwmapi.dll
        DwmEnableMMCSSPtr                           m_fnDwmEnableMMCSS;
        DwmSetDxFrameDurationPtr                    m_fnDwmSetDxFrameDuration;
        DwmSetPresentParametersPtr                  m_fnDwmSetPresentParameters;
        DwmGetCompositionTimingInfoPtr              m_fnDwmGetCompositionTimingInfo;
        DwmIsCompositionEnabledPtr                  m_fnDwmIsCompositionEnabled;
        DwmEnableCompositionPtr                     m_fnDwmEnableComposition;

        // Alternative VSync
        static __declspec(nothrow noalias) DWORD WINAPI VSyncThreadStatic(__in LPVOID lpParam);
        __declspec(nothrow noalias) __forceinline void VSyncThread();

        // retrieves the monitor EDID info
        __declspec(nothrow noalias noinline) void ReadDisplay();

        // display refresh rate reading functions
        __declspec(nothrow noalias noinline) bool GenericForExMode();

        // AMD specific (ADL library)
        __declspec(nothrow noalias noinline) bool SpecificForAMD();
#define ADL_MAX_PATH 256
#define ADL_OK 0
#define ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED 0x00000001
#define ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED 0x00000002
        struct AdapterInfo {int iSize, iAdapterIndex; char strUDID[ADL_MAX_PATH]; int iBusNumber, iDeviceNumber, iFunctionNumber, iVendorID; char strAdapterName[ADL_MAX_PATH], strDisplayName[ADL_MAX_PATH]; int iPresent, iExist; char strDriverPath[ADL_MAX_PATH], strDriverPathExt[ADL_MAX_PATH], strPNPString[ADL_MAX_PATH]; int iOSDisplayIndex;};
        struct ADLDisplayID {int iDisplayLogicalIndex, iDisplayPhysicalIndex, iDisplayLogicalAdapterIndex, iDisplayPhysicalAdapterIndex;};
        struct ADLDisplayInfo {ADLDisplayID displayID; int iDisplayControllerIndex; char strDisplayName[ADL_MAX_PATH], strDisplayManufacturerName[ADL_MAX_PATH]; int iDisplayType, iDisplayOutputType, iDisplayConnector, iDisplayInfoMask, iDisplayInfoValue;};
#ifdef _WIN64
        typedef void* (__cdecl* ADL_MAIN_MALLOC_CALLBACK)(size_t iSize);// modified, so it can take malloc() directly; __cdecl and __stdcall modify calling convention and function symbols on x86-32 only, when applied to 64-bit functions these merely modify the symbols
#else
        typedef void* (__stdcall* ADL_MAIN_MALLOC_CALLBACK)(int iSize);// not directly comptible with malloc() under x86-32
#endif
        typedef int (__cdecl* ADL_Adapter_NumberOfAdapters_GetPtr)(int* lpNumAdapters);
        typedef int (__cdecl* ADL_Adapter_AdapterInfo_GetPtr)(AdapterInfo* lpInfo, int iInputSize);
        typedef int (__cdecl* ADL_Display_DisplayInfo_GetPtr)(int iAdapterIndex, int* lpNumDisplays, ADLDisplayInfo** lppInfo, int iForceDetect);
        typedef int (__cdecl* ADL_Display_DDCBlockAccess_GetPtr)(int iAdapterIndex, int iDisplayIndex, int iOption, int iCommandIndex, int iSendMsgLen, unsigned __int8 const* lpucSendMsgBuf, int* lpulRecvMsgLen, unsigned __int8* lpucRecvMsgBuf);
        typedef int (__cdecl* ADL_Main_Control_DestroyPtr)();
        typedef int (__cdecl* ADL_Main_Control_CreatePtr)(ADL_MAIN_MALLOC_CALLBACK callback, int iEnumConnectedAdapters);

        union {
            struct {
                ADL_Adapter_NumberOfAdapters_GetPtr m_fnADL_Adapter_NumberOfAdapters_Get;
                ADL_Adapter_AdapterInfo_GetPtr m_fnADL_Adapter_AdapterInfo_Get;
                ADL_Display_DisplayInfo_GetPtr m_fnADL_Display_DisplayInfo_Get;
                ADL_Display_DDCBlockAccess_GetPtr m_fnADL_Display_DDCBlockAccess_Get;
                ADL_Main_Control_DestroyPtr m_fnADL_Main_Control_Destroy;
                ADL_Main_Control_CreatePtr m_fnADL_Main_Control_Create;
            } AMD;
        };
    };
}
