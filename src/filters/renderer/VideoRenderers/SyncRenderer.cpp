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
#include "../SyncClock/Interfaces.h"
#include <atlbase.h>
#include <atlcoll.h>
#include "../../../mpc-hc/resource.h"
#include "../../../DSUtil/DSUtil.h"
#include <strsafe.h> // Required in CGenlock
#include <videoacc.h>
#include <InitGuid.h>
#include <d3dx9.h>
#include <Mferror.h>
#include <vector>
#include "moreuuids.h"
#include "MacrovisionKicker.h"
#include "IPinHook.h"
#include "InternalDX9Shaders.h"
#include "SyncRenderer.h"
#include "version.h"
#include <intrin.h>

// only for debugging
//#define DISABLE_USING_D3D9EX

using namespace GothSync;

// Possible messages to the PowerStrip API. PowerStrip is used to control
// the display frequency in one of the video - display synchronization modes.
// Powerstrip can also through a CGenlock object give very accurate timing data
// (given) that the gfx board is supported by PS.
#define UM_SETCUSTOMTIMING (WM_USER+200)
#define UM_SETREFRESHRATE (WM_USER+201)
#define UM_SETPOLARITY (WM_USER+202)
#define UM_REMOTECONTROL (WM_USER+210)
#define UM_SETGAMMARAMP (WM_USER+203)
#define UM_CREATERESOLUTION (WM_USER+204)
#define UM_GETTIMING (WM_USER+205)
#define UM_SETCUSTOMTIMINGFAST (WM_USER+211) // Sets timing without writing to file. Faster

#define PositiveHorizontalPolarity 0x00
#define PositiveVerticalPolarity 0x00
#define NegativeHorizontalPolarity 0x02
#define NegativeVerticalPolarity 0x04
#define HideTrayIcon 0x00
#define ShowTrayIcon 0x01
#define ClosePowerStrip 0x63

#define HACTIVE 0
#define HFRONTPORCH 1
#define HSYNCWIDTH 2
#define HBACKPORCH 3
#define VACTIVE 4
#define VFRONTPORCH 5
#define VSYNCWIDTH 6
#define VBACKPORCH 7
#define PIXELCLOCK 8
#define UNKNOWN 9

__declspec(nothrow noalias) HRESULT CreateEVRS(HWND hWnd, CSyncAP** ppAP)
{
    ASSERT(ppAP);

#ifdef _M_X64
    void* pRawMem = malloc(sizeof(CSyncAP));
#else
    void* pRawMem = _aligned_malloc(sizeof(CSyncAP), 16);
#endif
    if (pRawMem) {
        HRESULT hr = E_OUTOFMEMORY;
        CString Error;
        CSyncAP* pAP = new(pRawMem) CSyncAP(hWnd, &hr, Error);
        if (FAILED(hr)) {
            pAP->Release();
            Error += L'\n';
            Error += GetWindowsErrorMessage(hr, nullptr);
            MessageBox(hWnd, Error, L"Error creating EVR Sync", MB_OK | MB_ICONERROR);
        } else if (!Error.IsEmpty()) {
            MessageBox(hWnd, Error, L"Warning creating EVR Sync", MB_OK | MB_ICONWARNING);
        }

        *ppAP = pAP;
        return hr;
    }
    MessageBox(hWnd, L"Out of memory for creating EVR Sync", nullptr, MB_OK | MB_ICONERROR);
    return E_OUTOFMEMORY;
}

CBaseAP::CBaseAP(HWND hWnd, HRESULT* phr, CString& _Error):
    CSubPicAllocatorPresenterImpl(hWnd),
    m_hDWMAPI(nullptr),
#ifdef D3D_DEBUG_INFO
    m_hD3D9(LoadLibraryW(L"d3d9d.dll")),
    m_hD3DX9Dll(LoadLibraryW(L"d3dx9d_43.dll")),
#else
    m_hD3D9(LoadLibraryW(L"d3d9.dll")),
    m_hD3DX9Dll(LoadLibraryW(L"d3dx9_43.dll")),
#endif
    m_hD3DCompiler(nullptr),
    m_u16DXSdkRelease(43),
    m_ScreenSize(0, 0),
    m_nDXSurface(1),
    m_nVMR9Surfaces(0),
    m_iVMR9Surface(0),
    m_nCurSurface(0),
    m_bSnapToVSync(false),
    m_upInterlaced(0),
    m_nUsedBuffer(0),
    m_upDX9ResizerTest(MAXSIZE_T),
    m_TextScale(1.0),
    m_dMainThreadId(0),
    m_hEvtQuit(INVALID_HANDLE_VALUE),
    m_upSyncGlitches(0),
    m_pGenlock(nullptr),
    m_lAudioLag(0),
    m_lAudioLagMin(10000),
    m_lAudioLagMax(-10000),
    m_pAudioStats(nullptr),
    m_upNextJitter(0),
    m_upNextSyncOffset(0),
    m_llLastSyncTime(0),
    m_dAverageFrameRate(0.0),
    m_dJitterStdDev(0.0),
    m_dSyncOffsetStdDev(0.0),
    m_dSyncOffsetAvr(0.0),
    m_llHysteresis(0),
    m_dD3DRefreshRate(0),
    m_dD3DRefreshCycle(0),
    m_dDetectedScanlineTime(0.0),
    m_dEstRefreshCycle(0.0),
    m_dFrameCycle(0.0),
    m_dOptimumDisplayCycle(0.0),
    m_dCycleDifference(1.0),
    m_llEstVBlankTime(0),
    m_bPendingResetDevice(false),
#pragma warning(disable: 4351)// the standard C4351 warning when default initializing arrays is irrelevant
    m_abVMR9AlphaBitmap(),
    m_llJitter(),
    m_llSyncOffset()
{
#if D3DX_SDK_VERSION != 43
#error DirectX SDK June 2010 (v43) is required to build this, if the DirectX SDK has been updated, add loading functions to this part of the code and the class initializer
#endif// this code has duplicates in ShaderEditorDlg.cpp, DX9AllocatorPresenter.cpp and SyncRenderer.cpp
    // load latest compatible version of the DLL that is available
    if (m_hD3DX9Dll) {
        m_hD3DCompiler = LoadLibraryW(L"D3DCompiler_43.dll");
    } else {
        _Error += L"The installed DirectX End-User Runtime is outdated. Please download and install the June 2010 release or newer in order for MPC-HC to function properly.\n";
        *phr = E_FAIL;
        return;
    }
    m_pD3DXCreateSprite = reinterpret_cast<D3DXCreateSpritePtr>(GetProcAddress(m_hD3DX9Dll, "D3DXCreateSprite"));// remove this one, it's useless
    m_pD3DXLoadSurfaceFromMemory = reinterpret_cast<D3DXLoadSurfaceFromMemoryPtr>(GetProcAddress(m_hD3DX9Dll, "D3DXLoadSurfaceFromMemory"));
    m_pD3DXLoadSurfaceFromSurface = reinterpret_cast<D3DXLoadSurfaceFromSurfacePtr>(GetProcAddress(m_hD3DX9Dll, "D3DXLoadSurfaceFromSurface"));
    m_pD3DXGetPixelShaderProfile = reinterpret_cast<D3DXGetPixelShaderProfilePtr>(GetProcAddress(m_hD3DX9Dll, "D3DXGetPixelShaderProfile"));
    m_pD3DXCreateLine = reinterpret_cast<D3DXCreateLinePtr>(GetProcAddress(m_hD3DX9Dll, "D3DXCreateLine"));
    m_pD3DXCreateFontW = reinterpret_cast<D3DXCreateFontWPtr>(GetProcAddress(m_hD3DX9Dll, "D3DXCreateFontW"));

    {
        // import function from D3DCompiler_43.dll
        static char const* const kszDcp = "D3DCompile";
        uintptr_t pModule = reinterpret_cast<uintptr_t>(m_hD3DCompiler);// just an named alias
        if (!pModule) {
            _Error += L"Could not find D3DCompiler_43.dll\n";
            *phr = E_FAIL;
            ASSERT(0);
            return;
        }
        IMAGE_DOS_HEADER const* pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER const*>(pModule);
        IMAGE_NT_HEADERS const* pNTHeader = reinterpret_cast<IMAGE_NT_HEADERS const*>(pModule + static_cast<size_t>(static_cast<ULONG>(pDOSHeader->e_lfanew)));
        IMAGE_EXPORT_DIRECTORY const* pEAT = reinterpret_cast<IMAGE_EXPORT_DIRECTORY const*>(pModule + static_cast<size_t>(pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress));
        uintptr_t pAONbase = pModule + static_cast<size_t>(pEAT->AddressOfNames);

        __declspec(align(8)) static char const kszFunc[] = "D3DCompile";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
        ptrdiff_t i = static_cast<size_t>(pEAT->NumberOfNames - 1);// convert to signed for the loop system and pointer-sized for the pointer operations
        for (;;) {
            unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
            char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
            if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                    && *reinterpret_cast<__int16 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int16 const*>(kszFunc + 8)
                    && kszName[10] == kszFunc[10]) {// note that this part must compare zero end inclusive
                // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                break;
            } else if (--i < 0) {
                ASSERT(0);
                _Error += L"Could not read data from D3DCompiler_43.dll.\n";
                *phr = E_FAIL;
                return;
            }
        }
        unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pModule + static_cast<size_t>(pEAT->AddressOfNameOrdinals) + i * 2);// table of two-byte elements
        unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pModule + static_cast<size_t>(pEAT->AddressOfFunctions) + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
        m_fnD3DCompile = reinterpret_cast<D3DCompilePtr>(pModule + static_cast<size_t>(u32AOF));
    }

    // store the OS version
    OSVERSIONINFOW version;
    version.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
    GetVersionExW(&version);
    //  m_u32OSversion = (version.dwMajorVersion<<24)|(version.dwMinorVersion<<16)|version.dwBuildNumber;

    typedef IDirect3D9 *(WINAPI * Direct3DCreate9Ptr)(__in UINT SDKVersion);
    if (version.dwMajorVersion >= 6) { // Vista and higher
        m_hDWMAPI = LoadLibraryW(L"dwmapi.dll");
        if (m_hDWMAPI) {
            m_pDwmIsCompositionEnabled = reinterpret_cast<DwmIsCompositionEnabledPtr>(GetProcAddress(m_hDWMAPI, "DwmIsCompositionEnabled"));
            m_pDwmEnableComposition = reinterpret_cast<DwmEnableCompositionPtr>(GetProcAddress(m_hDWMAPI, "DwmEnableComposition"));
        }

#ifndef DISABLE_USING_D3D9EX
        typedef HRESULT(WINAPI * Direct3DCreate9ExPtr)(__in UINT SDKVersion, __out IDirect3D9Ex** ppD3D);
        Direct3DCreate9ExPtr fnDirect3DCreate9Ex = reinterpret_cast<Direct3DCreate9ExPtr>(GetProcAddress(m_hD3D9, "Direct3DCreate9Ex"));

        if (FAILED(fnDirect3DCreate9Ex(D3D_SDK_VERSION, &m_pD3DEx))) {
            HRESULT hr = fnDirect3DCreate9Ex(D3D9b_SDK_VERSION, &m_pD3DEx);
            if (FAILED(hr)) {
                ASSERT(0);
                *phr = hr;
                _Error += L"Failed to create D3D9Ex\n";
                return;
            }
        }
        m_pD3D = m_pD3DEx;// no Addref() required
    }
#else
        Direct3DCreate9Ptr fnDirect3DCreate9 = reinterpret_cast<Direct3DCreate9Ptr>(GetProcAddress(m_hD3D9, "Direct3DCreate9"));

        m_pD3D = fnDirect3DCreate9(D3D_SDK_VERSION);
        if (!m_pD3D) { m_pD3D = fnDirect3DCreate9(D3D9b_SDK_VERSION); }
        if (!m_pD3D) {
            ASSERT(0);
            _Error += L"Failed to create D3D9\n";
            *phr = E_UNEXPECTED;
            return;
        }
    }
#endif
    else
    {
        Direct3DCreate9Ptr fnDirect3DCreate9 = reinterpret_cast<Direct3DCreate9Ptr>(GetProcAddress(m_hD3D9, "Direct3DCreate9"));

        m_pD3D = fnDirect3DCreate9(D3D_SDK_VERSION);
        if (!m_pD3D) { m_pD3D = fnDirect3DCreate9(D3D9b_SDK_VERSION); }
        if (!m_pD3D) {
            ASSERT(0);
            _Error += L"Failed to create D3D9\n";
            *phr = E_UNEXPECTED;
            return;
        }
    }

    m_upIsFullscreen = mk_pRendererSettings->bD3DFullscreen;

    if (mk_pRendererSettings->iVMRDisableDesktopComposition)
    {
        m_upDesktopCompositionDisabled = true;
        if (m_pDwmEnableComposition) {
            m_pDwmEnableComposition(0);
        }
    } else
    {
        m_upDesktopCompositionDisabled = false;
    }

    m_pGenlock = DEBUG_NEW CGenlock(mk_pRendererSettings, mk_pRendererSettings->fTargetSyncOffset, mk_pRendererSettings->fControlLimit, mk_pRendererSettings->iLineDelta, mk_pRendererSettings->iColumnDelta, mk_pRendererSettings->fCycleDelta, 0); // Must be done before CreateDXDevice
    *phr = CreateDXDevice(_Error);
}

CBaseAP::~CBaseAP()
{
    if (m_upDesktopCompositionDisabled) {
        m_upDesktopCompositionDisabled = false;
        if (m_pDwmEnableComposition) {
            m_pDwmEnableComposition(1);
        }
    }

    ULONG u;
    // release all subtitle resources
    if (m_pSubPicQueue) {
        u = m_pSubPicQueue->Release();
        ASSERT(!u);
    }
    if (m_pSubPicAllocator) {
        u = m_pSubPicAllocator->Release();
        ASSERT(!u);
    }
    if (m_pSubPicProvider) {// no assertion on the reference count, other filters may still hold a reference at this point
        m_pSubPicProvider->Release();
    }

    POSITION pos = m_apCustomPixelShaders[0].GetHeadPosition();
    while (pos) {
        EXTERNALSHADER& ceShader = m_apCustomPixelShaders[0].GetNext(pos);
        if (ceShader.pPixelShader) { ceShader.pPixelShader->Release(); }
        delete[] ceShader.strSrcData;
    }
    pos = m_apCustomPixelShaders[1].GetHeadPosition();
    while (pos) {
        EXTERNALSHADER& ceShader = m_apCustomPixelShaders[1].GetNext(pos);
        if (ceShader.pPixelShader) { ceShader.pPixelShader->Release(); }
        delete[] ceShader.strSrcData;
    }

    m_pSprite = nullptr;
    m_pFont = nullptr;
    m_pLine = nullptr;

    m_pD3DDev = nullptr;
    m_pD3DDevEx = nullptr;
    m_pD3D = nullptr;
    m_pD3DEx = nullptr;
    m_pAudioStats = nullptr;

    if (m_pGenlock) {
        delete m_pGenlock;
    }

    if (m_hDWMAPI) {
        FreeLibrary(m_hDWMAPI);
    }
    if (m_hD3DCompiler) {
        FreeLibrary(m_hD3DCompiler);
    }
    if (m_hD3DX9Dll) {
        FreeLibrary(m_hD3DX9Dll);
    }
    if (m_hD3D9) {
        FreeLibrary(m_hD3D9);
    }

}

template<unsigned texcoords>
static void AdjustQuad(MYD3DVERTEX<texcoords>* v, const float dx, const float dy)
{
    for (size_t i = 0; i < 4; ++i) {
        v[i].x -= 0.5f;
        v[i].y -= 0.5f;

        for (size_t j = 0; j < max(texcoords - 1, 1); ++j) {
            v[i].t[j].u -= 0.5f * dx;
            v[i].t[j].v -= 0.5f * dy;
        }

        if (texcoords > 1) {
            v[i].t[texcoords - 1].u -= 0.5f;
            v[i].t[texcoords - 1].v -= 0.5f;
        }
    }
}

template<unsigned texcoords>
static HRESULT TextureBlt(IDirect3DDevice9* m_pD3DDev, const MYD3DVERTEX<texcoords> v[4], const D3DTEXTUREFILTERTYPE filter)
{
    HRESULT hr;

    m_pD3DDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_pD3DDev->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pD3DDev->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pD3DDev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    m_pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_pD3DDev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    m_pD3DDev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    m_pD3DDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED);

    for (unsigned i = 0; i < texcoords; ++i) {
        m_pD3DDev->SetSamplerState(i, D3DSAMP_MAGFILTER, filter);
        m_pD3DDev->SetSamplerState(i, D3DSAMP_MINFILTER, filter);
        m_pD3DDev->SetSamplerState(i, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

        m_pD3DDev->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        m_pD3DDev->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    }

    // FVF = (texcoords == 1)? D3DFVF_TEX1 : (texcoords == 2)? D3DFVF_TEX2 : (texcoords == 3)? D3DFVF_TEX3 : (texcoords == 4)? D3DFVF_TEX4 : (texcoords == 5)? D3DFVF_TEX5 : (texcoords == 6)? D3DFVF_TEX6 : (texcoords == 7)? D3DFVF_TEX7 : (texcoords == 8)? D3DFVF_TEX8 : 0;
    hr = m_pD3DDev->SetFVF(D3DFVF_XYZRHW | texcoords * 0x100ui32); // this multiplication generates the same values as the FVF value above
    hr = m_pD3DDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(v[0]));

    // release textures
    for (unsigned i = 0; i < texcoords; ++i) { m_pD3DDev->SetTexture(i, nullptr); }

    return S_OK;
}

HRESULT CBaseAP::DrawRectBase(IDirect3DDevice9* pD3DDev, MYD3DVERTEX<0> v[4])
{
    HRESULT hr = pD3DDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    hr = pD3DDev->SetRenderState(D3DRS_LIGHTING, FALSE);
    hr = pD3DDev->SetRenderState(D3DRS_ZENABLE, FALSE);
    hr = pD3DDev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    hr = pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    hr = pD3DDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    hr = pD3DDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    hr = pD3DDev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    hr = pD3DDev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

    hr = pD3DDev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED);

    hr = pD3DDev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX0 | D3DFVF_DIFFUSE);

    MYD3DVERTEX<0> tmp = v[2];
    v[2] = v[3];
    v[3] = tmp;
    hr = pD3DDev->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, v, sizeof(v[0]));

    return S_OK;
}

MFOffset CBaseAP::GetOffset(float v)
{
    MFOffset offset;
    offset.value = short(v);
    offset.fract = WORD(65536 * (v - offset.value));
    return offset;
}

MFVideoArea CBaseAP::GetArea(float x, float y, DWORD width, DWORD height)
{
    MFVideoArea area;
    area.OffsetX = GetOffset(x);
    area.OffsetY = GetOffset(y);
    area.Area.cx = width;
    area.Area.cy = height;
    return area;
}

void CBaseAP::ResetStats()
{
    m_pGenlock->ResetStats();
    m_lAudioLag = 0;
    m_lAudioLagMin = 10000;
    m_lAudioLagMax = -10000;
    m_llMinJitter = MAXINT64;
    m_llMaxJitter = MINLONG64;
    m_llMinSyncOffset = MAXINT64;
    m_llMaxSyncOffset = MINLONG64;
    m_upSyncGlitches = 0;
    m_pcFramesDropped = 0;
}

bool CBaseAP::SettingsNeedResetDevice()
{
    bool bRet = false;
    if (!m_upIsFullscreen) {
        if (mk_pRendererSettings->iVMRDisableDesktopComposition) {
            if (!m_upDesktopCompositionDisabled) {
                m_upDesktopCompositionDisabled = true;
                if (m_pDwmEnableComposition) {
                    m_pDwmEnableComposition(0);
                }
            }
        } else {
            if (m_upDesktopCompositionDisabled) {
                m_upDesktopCompositionDisabled = false;
                if (m_pDwmEnableComposition) {
                    m_pDwmEnableComposition(1);
                }
            }
        }
    }
    bRet = bRet || mk_pRendererSettings->iVMR9HighColorResolution != m_bVMR9HighColorResolutionCurrent;
    m_bVMR9HighColorResolutionCurrent = mk_pRendererSettings->iVMR9HighColorResolution;
    return bRet;
}

HRESULT CBaseAP::CreateDXDevice(CString& _Error)
{
    TRACE(L"--> CBaseAP::CreateDXDevice on thread: %d\n", GetCurrentThreadId());
    m_bVMR9HighColorResolutionCurrent = mk_pRendererSettings->iVMR9HighColorResolution;
    HRESULT hr = E_FAIL;

    m_pFont = nullptr;
    m_pSprite = nullptr;
    m_pLine = nullptr;

    m_pD3DDev = nullptr;
    m_pD3DDevEx = nullptr;

    m_pResizerPixelShaderX = m_pResizerPixelShaderY = nullptr;

    POSITION pos = m_apCustomPixelShaders[0].GetHeadPosition();
    while (pos) {
        EXTERNALSHADER& ceShader = m_apCustomPixelShaders[0].GetNext(pos);
        if (ceShader.pPixelShader) {
            ceShader.pPixelShader->Release();
            ceShader.pPixelShader = nullptr;
        }
    }
    pos = m_apCustomPixelShaders[1].GetHeadPosition();
    while (pos) {
        EXTERNALSHADER& ceShader = m_apCustomPixelShaders[1].GetNext(pos);
        if (ceShader.pPixelShader) {
            ceShader.pPixelShader->Release();
            ceShader.pPixelShader = nullptr;
        }
    }

    if (!m_pD3D) {
        _Error += L"Failed to create Direct3D device\n";
        return E_UNEXPECTED;
    }

    D3DDISPLAYMODE d3ddm;
    ZeroMemory(&d3ddm, sizeof(d3ddm));
    m_uiCurrentAdapter = GetAdapter(m_pD3D);
    if (FAILED(m_pD3D->GetAdapterDisplayMode(m_uiCurrentAdapter, &d3ddm))) {
        _Error += L"Can not retrieve display mode data\n";
        return E_UNEXPECTED;
    }

    if (FAILED(m_pD3D->GetDeviceCaps(m_uiCurrentAdapter, D3DDEVTYPE_HAL, &m_dcCaps)))
        if (!(m_dcCaps.Caps & D3DCAPS_READ_SCANLINE)) {
            _Error += L"Video card does not have scanline access. Display synchronization is not possible.\n";
            return E_UNEXPECTED;
        }

    if (d3ddm.RefreshRate == 119 || d3ddm.RefreshRate == 89 || d3ddm.RefreshRate == 71 || d3ddm.RefreshRate == 59 || d3ddm.RefreshRate == 47 || d3ddm.RefreshRate == 29 || d3ddm.RefreshRate == 23) {
        m_dD3DRefreshRate = (d3ddm.RefreshRate + 1) / 1.001; // NTSC adapted
    } else {
        m_dD3DRefreshRate = d3ddm.RefreshRate;// exact amounts
    }
    m_dD3DRefreshCycle = 1000.0 / m_dD3DRefreshRate; // In ms
    m_ScreenSize.SetSize(d3ddm.Width, d3ddm.Height);
    m_pGenlock->SetDisplayResolution(d3ddm.Width, d3ddm.Height);

    BOOL boCompositionEnabled = 0;
    if (m_pDwmIsCompositionEnabled) {
        m_pDwmIsCompositionEnabled(&boCompositionEnabled);
    }
    m_boCompositionEnabled = boCompositionEnabled;

    ZeroMemory(&pp, sizeof(pp));
    if (m_upIsFullscreen) { // Exclusive mode fullscreen
        pp.Windowed = FALSE;
        m_u32WindowWidth = pp.BackBufferWidth = d3ddm.Width;
        m_u32WindowHeight = pp.BackBufferHeight = d3ddm.Height;
        pp.hDeviceWindow = m_hVideoWnd;
        DEBUG_ONLY(_tprintf_s(_T("Wnd in CreateDXDevice: %d\n"), m_hVideoWnd));
        pp.BackBufferCount = 3;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        pp.Flags = D3DPRESENTFLAG_VIDEO;
        m_upHighColorResolution = mk_pRendererSettings->iVMR9HighColorResolution;
        if (m_upHighColorResolution) {
            if (FAILED(m_pD3D->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, D3DFMT_A2R10G10B10, false))) {
                m_strStatsMsg[MSG_ERROR] = L"10 bit RGB is not supported by this graphics device in this resolution.";
                m_upHighColorResolution = false;
            }
        }

        if (m_upHighColorResolution) {
            pp.BackBufferFormat = D3DFMT_A2R10G10B10;
        } else {
            pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        }

        if (m_pD3DEx) {
            D3DDISPLAYMODEEX DisplayMode;
            ZeroMemory(&DisplayMode, sizeof(DisplayMode));
            DisplayMode.Size = sizeof(DisplayMode);
            m_pD3DEx->GetAdapterDisplayModeEx(m_uiCurrentAdapter, &DisplayMode, nullptr);

            DisplayMode.Format = pp.BackBufferFormat;
            pp.FullScreen_RefreshRateInHz = DisplayMode.RefreshRate;

            if FAILED(hr = m_pD3DEx->CreateDeviceEx(m_uiCurrentAdapter, D3DDEVTYPE_HAL, m_hVideoWnd,
                                                    D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED | D3DCREATE_ENABLE_PRESENTSTATS,
                                                    &pp, &DisplayMode, &m_pD3DDevEx)) {
                _Error += GetWindowsErrorMessage(hr, nullptr);
                return hr;
            }
            if (m_pD3DDevEx) {
                m_pD3DDev = m_pD3DDevEx;
                m_dfDisplayType = DisplayMode.Format;
            }
        } else {
            if FAILED(hr = m_pD3D->CreateDevice(m_uiCurrentAdapter, D3DDEVTYPE_HAL, m_hVideoWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED, &pp, &m_pD3DDev)) {
                _Error += GetWindowsErrorMessage(hr, nullptr);
                return hr;
            }
            DEBUG_ONLY(_tprintf_s(_T("Created full-screen device\n")));
            if (m_pD3DDev) {
                m_dfDisplayType = d3ddm.Format;
            }
        }
    } else { // Windowed
        pp.Windowed = TRUE;
        pp.hDeviceWindow = m_hVideoWnd;
        pp.SwapEffect = D3DSWAPEFFECT_COPY;
        pp.Flags = D3DPRESENTFLAG_VIDEO;
        pp.BackBufferCount = 1;
        m_u32WindowWidth = pp.BackBufferWidth = d3ddm.Width;
        m_u32WindowHeight = pp.BackBufferHeight = d3ddm.Height;
        m_dfDisplayType = d3ddm.Format;
        m_upHighColorResolution = mk_pRendererSettings->iVMR9HighColorResolution;
        if (m_upHighColorResolution) {
            if (FAILED(m_pD3D->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_A2R10G10B10, D3DFMT_A2R10G10B10, false))) {
                m_strStatsMsg[MSG_ERROR] = L"10 bit RGB is not supported by this graphics device in this resolution.";
                m_upHighColorResolution = false;
            }
        }

        if (m_upHighColorResolution) {
            pp.BackBufferFormat = D3DFMT_A2R10G10B10;// won't work, needs to adapt the device creation fullscreen display mode as well
        }
        if (boCompositionEnabled) {
            // Desktop composition presents the whole desktop
            pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        } else {
            pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        }
        if (m_pD3DEx) {
            if FAILED(hr = m_pD3DEx->CreateDeviceEx(m_uiCurrentAdapter, D3DDEVTYPE_HAL, m_hVideoWnd,
                                                    D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED | D3DCREATE_ENABLE_PRESENTSTATS,
                                                    &pp, nullptr, &m_pD3DDevEx)) {
                _Error += GetWindowsErrorMessage(hr, nullptr);
                return hr;
            }
            if (m_pD3DDevEx) {
                m_pD3DDev = m_pD3DDevEx;
            }
        } else {
            if FAILED(hr = m_pD3D->CreateDevice(m_uiCurrentAdapter, D3DDEVTYPE_HAL, m_hVideoWnd,
                                                D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED,
                                                &pp, &m_pD3DDev)) {
                _Error += GetWindowsErrorMessage(hr, nullptr);
                return hr;
            }
            DEBUG_ONLY(_tprintf_s(_T("Created windowed device\n")));
        }
    }

    while (hr == D3DERR_DEVICELOST) {
        TRACE(L"D3DERR_DEVICELOST. Trying to Reset.\n");
        hr = m_pD3DDev->TestCooperativeLevel();
    }
    if (hr == D3DERR_DEVICENOTRESET) {
        TRACE(L"D3DERR_DEVICENOTRESET\n");
        hr = m_pD3DDev->Reset(&pp);
    }

    TRACE(L"CreateDevice(): %d\n", (LONG)hr);
    ASSERT(hr == S_OK);

    if (m_pD3DDevEx) {
        m_pD3DDevEx->SetGPUThreadPriority(7);
    }

    double dWidth = static_cast<double>(static_cast<__int32>(m_u32WindowWidth)), dHeight = static_cast<double>(static_cast<__int32>(m_u32WindowHeight));// the standard converter only does a proper job with signed values
    if (m_dWindowWidth != dWidth || m_dWindowHeight != dHeight) { // backbuffer size changed, reset the CDX9SubPicAllocator and stats screen font
        // screenspace size parts
        m_dWindowWidth = dWidth;
        m_dWindowHeight = dHeight;
        m_fWindowWidth = static_cast<float>(m_dWindowWidth);
        m_fWindowHeight = static_cast<float>(m_dWindowHeight);
        m_fWindowWidthr = static_cast<float>(1.0 / m_dWindowWidth);
        m_fWindowHeightr = static_cast<float>(1.0 / m_dWindowHeight);
        m_pFont = nullptr;
    }

    // create subtitle renderer resources
    __int32 i32Oleft, i32Otop;
    unsigned __int32 u32sx, u32sy;
    if (mk_pRendererSettings->bPositionRelative) {
        i32Oleft = m_VideoRect.left;
        i32Otop = m_VideoRect.top;
        u32sx = m_VideoRect.right - m_VideoRect.left;
        u32sy = m_VideoRect.bottom - m_VideoRect.top;
    } else {
        i32Oleft = 0;
        i32Otop = 0;
        u32sx = m_u32WindowWidth;
        u32sy = m_u32WindowHeight;
    }
    if (mk_pRendererSettings->nSPCMaxRes) {// half and tree-quarters resolution
        i32Oleft >>= 1;
        i32Otop >>= 1;
        u32sx >>= 1;
        u32sy >>= 1;
        if (mk_pRendererSettings->nSPCMaxRes == 1) {// tree-quarters resolution
            i32Oleft += i32Oleft >> 1;
            i32Otop += i32Otop >> 1;
            u32sx += u32sx >> 1;
            u32sy += u32sy >> 1;
        }
    }
    m_i32SubWindowOffsetLeft = i32Oleft;
    m_i32SubWindowOffsetTop = i32Otop;

    if (m_pSubPicQueue) {
        m_pSubPicQueue->Release();
        m_pSubPicQueue = nullptr;
    }
    if (m_pSubPicAllocator) {
        m_pSubPicAllocator->Release();
    }

    void* pRawMem = malloc(sizeof(CDX9SubPicAllocator));
    if (!pRawMem) {
        _Error = L"Out of memory error for creating CDX9SubPicAllocator\n";
        ASSERT(0);
        return E_OUTOFMEMORY;
    }
    CDX9SubPicAllocator* pSubPicAllocator = new(pRawMem) CDX9SubPicAllocator(u32sx, u32sy, m_pD3DDev);
    m_pSubPicAllocator = static_cast<CSubPicAllocatorImpl*>(pSubPicAllocator);// reference inherited

    CSubPicQueueImpl* pSubPicQueue;
    if (mk_pRendererSettings->nSPCSize) {
        pRawMem = malloc(sizeof(CSubPicQueue));
        if (!pRawMem) {
            _Error = L"Out of memory error for creating CSubPicQueue\n";
            ASSERT(0);
            return E_OUTOFMEMORY;
        }
        pSubPicQueue = static_cast<CSubPicQueueImpl*>(new(pRawMem) CSubPicQueue(m_pSubPicAllocator, m_dDetectedVideoFrameRate, mk_pRendererSettings->nSPCSize, !mk_pRendererSettings->fSPCAllowAnimationWhenBuffering));
    } else {
        pRawMem = malloc(sizeof(CSubPicQueueNoThread));
        if (!pRawMem) {
            _Error = L"Out of memory error for creating CSubPicQueueNoThread\n";
            ASSERT(0);
            return E_OUTOFMEMORY;
        }
        pSubPicQueue = static_cast<CSubPicQueueImpl*>(new(pRawMem) CSubPicQueueNoThread(m_pSubPicAllocator, m_dDetectedVideoFrameRate));
    }
    m_pSubPicQueue = pSubPicQueue;// reference inherited
    if (m_pSubPicProvider) {
        pSubPicQueue->SetSubPicProvider(m_pSubPicProvider);
    }

    m_pProfile = m_pD3DXGetPixelShaderProfile(m_pD3DDev);// get the pixel shader profile level

    if (m_pD3DXCreateFontW) {
        int MinSize = 1600;
        int CurrentSize = min(m_ScreenSize.cx, MinSize);
        double Scale = double(CurrentSize) / double(MinSize);
        m_TextScale = Scale;
        m_pD3DXCreateFontW(m_pD3DDev, -24.0 * Scale, -11.0 * Scale, CurrentSize < 800 ? FW_NORMAL : FW_BOLD, 1, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Lucida Console", &m_pFont);
    }
    if (m_pD3DXCreateSprite) {
        m_pD3DXCreateSprite(m_pD3DDev, &m_pSprite);
    }
    if (m_pD3DXCreateLine) {
        m_pD3DXCreateLine(m_pD3DDev, &m_pLine);
    }
    m_llLastAdapterCheck = PerfCounter100ns();
    return S_OK;
}

HRESULT CBaseAP::ResetDXDevice(CString& _Error)
{
    m_bVMR9HighColorResolutionCurrent = mk_pRendererSettings->iVMR9HighColorResolution;
    HRESULT hr = E_FAIL;

    hr = m_pD3DDev->TestCooperativeLevel();
    if ((hr != D3DERR_DEVICENOTRESET) && (hr != D3D_OK)) {
        return hr;
    }

    CComPtr<IEnumPins> rendererInputEnum;
    std::vector<CComPtr<IPin>> decoderOutput;
    std::vector<CComPtr<IPin>> rendererInput;
    FILTER_INFO filterInfo;

    bool disconnected = FALSE;

    // Disconnect all pins to release video memory resources
    if (m_pD3DDev) {
        ZeroMemory(&filterInfo, sizeof(filterInfo));
        m_pOuterEVR->QueryFilterInfo(&filterInfo); // This addref's the pGraph member
        if (SUCCEEDED(m_pOuterEVR->EnumPins(&rendererInputEnum))) {
            CComPtr<IPin> input;
            CComPtr<IPin> output;
            while (hr = rendererInputEnum->Next(1, &input.p, 0), hr == S_OK) { // Must have .p here
                DEBUG_ONLY(_tprintf_s(_T("Pin found\n")));
                input->ConnectedTo(&output.p);
                if (output != nullptr) {
                    rendererInput.push_back(input);
                    decoderOutput.push_back(output);
                }
                input.Release();
                output.Release();
            }
        } else {
            return hr;
        }
        for (size_t i = 0; i < decoderOutput.size(); i++) {
            DEBUG_ONLY(_tprintf_s(_T("Disconnecting pin\n")));
            filterInfo.pGraph->Disconnect(decoderOutput.at(i).p);
            filterInfo.pGraph->Disconnect(rendererInput.at(i).p);
            DEBUG_ONLY(_tprintf_s(_T("Pin disconnected\n")));
        }
        disconnected = true;
    }

    // Release more resources
    m_pFont = nullptr;
    m_pSprite = nullptr;
    m_pLine = nullptr;

    m_pResizerPixelShaderX = m_pResizerPixelShaderY = nullptr;

    POSITION pos = m_apCustomPixelShaders[0].GetHeadPosition();
    while (pos) {
        EXTERNALSHADER& ceShader = m_apCustomPixelShaders[0].GetNext(pos);
        if (ceShader.pPixelShader) {
            ceShader.pPixelShader->Release();
            ceShader.pPixelShader = nullptr;
        }
    }
    pos = m_apCustomPixelShaders[1].GetHeadPosition();
    while (pos) {
        EXTERNALSHADER& ceShader = m_apCustomPixelShaders[1].GetNext(pos);
        if (ceShader.pPixelShader) {
            ceShader.pPixelShader->Release();
            ceShader.pPixelShader = nullptr;
        }
    }

    D3DDISPLAYMODE d3ddm;
    ZeroMemory(&d3ddm, sizeof(d3ddm));
    if (FAILED(m_pD3D->GetAdapterDisplayMode(GetAdapter(m_pD3D), &d3ddm))) {
        _Error += L"Can not retrieve display mode data\n";
        return E_UNEXPECTED;
    }

    if (d3ddm.RefreshRate == 119 || d3ddm.RefreshRate == 89 || d3ddm.RefreshRate == 71 || d3ddm.RefreshRate == 59 || d3ddm.RefreshRate == 47 || d3ddm.RefreshRate == 29 || d3ddm.RefreshRate == 23) {
        m_dD3DRefreshRate = (d3ddm.RefreshRate + 1) / 1.001; // NTSC adapted
    } else {
        m_dD3DRefreshRate = d3ddm.RefreshRate;// exact amounts
    }
    m_dD3DRefreshCycle = 1000.0 / m_dD3DRefreshRate; // In ms
    m_ScreenSize.SetSize(d3ddm.Width, d3ddm.Height);
    m_pGenlock->SetDisplayResolution(d3ddm.Width, d3ddm.Height);

    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));

    BOOL boCompositionEnabled = false;
    if (m_pDwmIsCompositionEnabled) {
        m_pDwmIsCompositionEnabled(&boCompositionEnabled);
    }
    m_boCompositionEnabled = boCompositionEnabled;
    m_upHighColorResolution = mk_pRendererSettings->iVMR9HighColorResolution;

    if (m_upIsFullscreen) { // Exclusive mode fullscreen
        pp.BackBufferWidth = d3ddm.Width;
        pp.BackBufferHeight = d3ddm.Height;
        if (m_upHighColorResolution) {
            pp.BackBufferFormat = D3DFMT_A2R10G10B10;
        } else {
            pp.BackBufferFormat = d3ddm.Format;
        }
        if (FAILED(m_pD3DEx->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, pp.BackBufferFormat, pp.BackBufferFormat, false))) {
            _Error += L"10 bit RGB is not supported by this graphics device in exclusive mode fullscreen.\n";
            return hr;
        }

        D3DDISPLAYMODEEX DisplayMode;
        ZeroMemory(&DisplayMode, sizeof(DisplayMode));
        DisplayMode.Size = sizeof(DisplayMode);
        if (m_pD3DDevEx) {
            m_pD3DEx->GetAdapterDisplayModeEx(GetAdapter(m_pD3DEx), &DisplayMode, nullptr);
            DisplayMode.Format = pp.BackBufferFormat;
            pp.FullScreen_RefreshRateInHz = DisplayMode.RefreshRate;
            if FAILED(m_pD3DDevEx->Reset(&pp)) {
                _Error += GetWindowsErrorMessage(hr, nullptr);
                return hr;
            }
        } else if (m_pD3DDev) {
            if FAILED(m_pD3DDev->Reset(&pp)) {
                _Error += GetWindowsErrorMessage(hr, nullptr);
                return hr;
            }
        } else {
            _Error += L"No device.\n";
            return hr;
        }
        m_dfDisplayType = d3ddm.Format;
    } else { // Windowed
        m_u32WindowWidth = pp.BackBufferWidth = d3ddm.Width;
        m_u32WindowHeight = pp.BackBufferHeight = d3ddm.Height;
        m_dfDisplayType = d3ddm.Format;
        if (m_upHighColorResolution) {
            pp.BackBufferFormat = D3DFMT_A2R10G10B10;// won't work, needs to adapt the device creation fullscreen display mode as well
        }
        if (FAILED(m_pD3DEx->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, pp.BackBufferFormat, pp.BackBufferFormat, false))) {
            _Error += L"10 bit RGB is not supported by this graphics device in windowed mode.\n";
            return hr;
        }
        if (boCompositionEnabled) {
            // Desktop composition presents the whole desktop
            pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        } else {
            pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        }
        if (m_pD3DDevEx)
            if FAILED(m_pD3DDevEx->Reset(&pp)) {
                _Error += GetWindowsErrorMessage(hr, nullptr);
                return hr;
            } else if (m_pD3DDev)
                if FAILED(m_pD3DDevEx->Reset(&pp)) {
                    _Error += GetWindowsErrorMessage(hr, nullptr);
                    return hr;
                } else {
                    _Error += L"No device.\n";
                    return hr;
                }
    }

    if (disconnected) {
        for (size_t i = 0; i < decoderOutput.size(); i++) {
            if (FAILED(filterInfo.pGraph->ConnectDirect(decoderOutput.at(i).p, rendererInput.at(i).p, nullptr))) {
                return hr;
            }
        }

        if (filterInfo.pGraph != nullptr) {
            filterInfo.pGraph->Release();
        }
    }

    double dWidth = static_cast<double>(static_cast<__int32>(m_u32WindowWidth)), dHeight = static_cast<double>(static_cast<__int32>(m_u32WindowHeight));// the standard converter only does a proper job with signed values
    if (m_dWindowWidth != dWidth || m_dWindowHeight != dHeight) { // backbuffer size changed, reset the CDX9SubPicAllocator and stats screen font
        // screenspace size parts
        m_dWindowWidth = dWidth;
        m_dWindowHeight = dHeight;
        m_fWindowWidth = static_cast<float>(m_dWindowWidth);
        m_fWindowHeight = static_cast<float>(m_dWindowHeight);
        m_fWindowWidthr = static_cast<float>(1.0 / m_dWindowWidth);
        m_fWindowHeightr = static_cast<float>(1.0 / m_dWindowHeight);
        m_pFont = nullptr;
    }

    // create subtitle renderer resources
    __int32 i32Oleft, i32Otop;
    unsigned __int32 u32sx, u32sy;
    if (mk_pRendererSettings->bPositionRelative) {
        i32Oleft = m_VideoRect.left;
        i32Otop = m_VideoRect.top;
        u32sx = m_VideoRect.right - m_VideoRect.left;
        u32sy = m_VideoRect.bottom - m_VideoRect.top;
    } else {
        i32Oleft = 0;
        i32Otop = 0;
        u32sx = m_u32WindowWidth;
        u32sy = m_u32WindowHeight;
    }
    if (mk_pRendererSettings->nSPCMaxRes) {// half and tree-quarters resolution
        i32Oleft >>= 1;
        i32Otop >>= 1;
        u32sx >>= 1;
        u32sy >>= 1;
        if (mk_pRendererSettings->nSPCMaxRes == 1) {// tree-quarters resolution
            i32Oleft += i32Oleft >> 1;
            i32Otop += i32Otop >> 1;
            u32sx += u32sx >> 1;
            u32sy += u32sy >> 1;
        }
    }
    m_i32SubWindowOffsetLeft = i32Oleft;
    m_i32SubWindowOffsetTop = i32Otop;

    if (m_pSubPicQueue) {
        m_pSubPicQueue->Release();
        m_pSubPicQueue = nullptr;
    }
    if (m_pSubPicAllocator) {
        m_pSubPicAllocator->Release();
    }

    void* pRawMem = malloc(sizeof(CDX9SubPicAllocator));
    if (!pRawMem) {
        _Error = L"Out of memory error for creating CDX9SubPicAllocator\n";
        ASSERT(0);
        return E_OUTOFMEMORY;
    }
    CDX9SubPicAllocator* pSubPicAllocator = new(pRawMem) CDX9SubPicAllocator(u32sx, u32sy, m_pD3DDev);
    m_pSubPicAllocator = static_cast<CSubPicAllocatorImpl*>(pSubPicAllocator);
    pSubPicAllocator->AddRef();

    CSubPicQueueImpl* pSubPicQueue;
    if (mk_pRendererSettings->nSPCSize) {
        pRawMem = malloc(sizeof(CSubPicQueue));
        if (!pRawMem) {
            _Error = L"Out of memory error for creating CSubPicQueue\n";
            ASSERT(0);
            return E_OUTOFMEMORY;
        }
        pSubPicQueue = static_cast<CSubPicQueueImpl*>(new(pRawMem) CSubPicQueue(m_pSubPicAllocator, m_dDetectedVideoFrameRate, mk_pRendererSettings->nSPCSize, !mk_pRendererSettings->fSPCAllowAnimationWhenBuffering));
    } else {
        pRawMem = malloc(sizeof(CSubPicQueueNoThread));
        if (!pRawMem) {
            _Error = L"Out of memory error for creating CSubPicQueueNoThread\n";
            ASSERT(0);
            return E_OUTOFMEMORY;
        }
        pSubPicQueue = static_cast<CSubPicQueueImpl*>(new(pRawMem) CSubPicQueueNoThread(m_pSubPicAllocator, m_dDetectedVideoFrameRate));
    }
    m_pSubPicQueue = pSubPicQueue;
    pSubPicQueue->AddRef();
    if (m_pSubPicProvider) {
        m_pSubPicQueue->SetSubPicProvider(m_pSubPicProvider);
    }

    m_pProfile = m_pD3DXGetPixelShaderProfile(m_pD3DDev);// get the pixel shader profile level

    m_pFont = nullptr;
    if (m_pD3DXCreateFontW) {
        int MinSize = 1600;
        int CurrentSize = min(m_ScreenSize.cx, MinSize);
        double Scale = double(CurrentSize) / double(MinSize);
        m_TextScale = Scale;
        m_pD3DXCreateFontW(m_pD3DDev, -24.0 * Scale, -11.0 * Scale, CurrentSize < 800 ? FW_NORMAL : FW_BOLD, 0, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Lucida Console", &m_pFont);
    }
    m_pSprite = nullptr;
    if (m_pD3DXCreateSprite) {
        m_pD3DXCreateSprite(m_pD3DDev, &m_pSprite);
    }
    m_pLine = nullptr;
    if (m_pD3DXCreateLine) {
        m_pD3DXCreateLine(m_pD3DDev, &m_pLine);
    }
    return S_OK;
}

HRESULT CBaseAP::AllocSurfaces(D3DFORMAT Format)
{
    CAutoLock cRenderLock(&m_csAllocatorLock);

    // video size parts
    //m_u32VideoWidth = m_NativeVideoSize.cx;
    //m_u32VideoHeight = m_NativeVideoSize.cy;
    m_dVideoWidth = static_cast<double>(static_cast<__int32>(m_u32VideoWidth));// the standard converter only does a proper job with signed values
    m_dVideoHeight = static_cast<double>(static_cast<__int32>(m_u32VideoHeight));
    m_fVideoWidth = static_cast<float>(m_dVideoWidth);
    m_fVideoHeight = static_cast<float>(m_dVideoHeight);
    m_fVideoWidthr = static_cast<float>(1.0 / m_dVideoWidth);
    m_fVideoHeightr = static_cast<float>(1.0 / m_dVideoHeight);

    ptrdiff_t k = MAX_PICTURE_SLOTS - 1;
    do {
        m_apVideoSurface[k] = nullptr;
        m_apVideoTexture[k] = nullptr;
    } while (--k >= 0);

    m_pScreenSizeTemporaryTexture[0] = m_pScreenSizeTemporaryTexture[1] = nullptr;
    m_dfSurfaceType = Format;

    HRESULT hr;
    size_t nTexturesNeeded = m_nDXSurface + 2;

    for (size_t i = 0; i < nTexturesNeeded; i++) {
        if (FAILED(hr = m_pD3DDev->CreateTexture(m_u32VideoWidth, m_u32VideoHeight, 1, D3DUSAGE_RENDERTARGET, m_dfSurfaceType, D3DPOOL_DEFAULT, &m_apVideoTexture[i], nullptr))) { return hr; }
        if (FAILED(hr = m_apVideoTexture[i]->GetSurfaceLevel(0, &m_apVideoSurface[i]))) { return hr; }
    }

    for (size_t j = 0; j < 2; ++j) {
        if (FAILED(hr = m_pD3DDev->CreateTexture(m_u32WindowWidth, m_u32WindowHeight, 1, D3DUSAGE_RENDERTARGET, m_dfSurfaceType, D3DPOOL_DEFAULT, &m_pScreenSizeTemporaryTexture[j], nullptr))) { return hr; }
    }

    hr = m_pD3DDev->Clear(0, nullptr, D3DCLEAR_TARGET, mk_pRendererSettings->dwBackgoundColor, 1.0f, 0);
    return S_OK;
}

void CBaseAP::DeleteSurfaces()
{
    CAutoLock cRenderLock(&m_csAllocatorLock);

    for (size_t i = 0; i < m_nDXSurface + 2; i++) {
        m_apVideoTexture[i] = nullptr;
        m_apVideoSurface[i] = nullptr;
    }
}

UINT CBaseAP::GetAdapter(IDirect3D9* pD3D)
{
    if (m_hVideoWnd == nullptr || pD3D == nullptr) {
        return D3DADAPTER_DEFAULT;
    }

    HMONITOR hMonitor = MonitorFromWindow(m_hVideoWnd, MONITOR_DEFAULTTONEAREST);
    if (hMonitor == nullptr) {
        return D3DADAPTER_DEFAULT;
    }

    for (UINT adp = 0, num_adp = pD3D->GetAdapterCount(); adp < num_adp; ++adp) {
        HMONITOR hAdpMon = pD3D->GetAdapterMonitor(adp);
        if (hAdpMon == hMonitor) {
            return adp;
        }
    }
    return D3DADAPTER_DEFAULT;
}

// CSubPicAllocatorPresenterImpl

bool CBaseAP::ClipToSurface(IDirect3DSurface9* pSurface, CRect& s, CRect& d)
{
    D3DSURFACE_DESC d3dsd;
    ZeroMemory(&d3dsd, sizeof(d3dsd));
    if (FAILED(pSurface->GetDesc(&d3dsd))) {
        return false;
    }

    int w = d3dsd.Width, h = d3dsd.Height;
    int sw = s.Width(), sh = s.Height();
    int dw = d.Width(), dh = d.Height();

    if (d.left >= w || d.right < 0 || d.top >= h || d.bottom < 0
            || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) {
        s.SetRectEmpty();
        d.SetRectEmpty();
        return true;
    }
    if (d.right > w) {
        s.right -= (d.right - w) * sw / dw;
        d.right = w;
    }
    if (d.bottom > h) {
        s.bottom -= (d.bottom - h) * sh / dh;
        d.bottom = h;
    }
    if (d.left < 0) {
        s.left += (0 - d.left) * sw / dw;
        d.left = 0;
    }
    if (d.top < 0) {
        s.top += (0 - d.top) * sh / dh;
        d.top = 0;
    }
    return true;
}

__forceinline HRESULT CBaseAP::CompileShader(__in char const* szSrcData, __in size_t srcDataLen, __in char const* szFunctionName, __out_opt IDirect3DPixelShader9** ppPixelShader) const
{
    HRESULT hr;
    ID3DBlob* pBlob;
    if (FAILED(hr = m_fnD3DCompile(szSrcData, srcDataLen, nullptr, nullptr, nullptr, szFunctionName, m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &pBlob, nullptr))) { return hr; }
    hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(pBlob->GetBufferPointer()), ppPixelShader);
    pBlob->Release();
    return hr;
}

HRESULT CBaseAP::InitResizers(const int iDX9Resizer)
{
    // check whether the resizer pixel shaders and the intermediate resizer surface must be initialized
    if ((m_upDX9ResizerTest != iDX9Resizer) || (m_xformTest != m_xform) || (m_rcVideoRectTest.left != m_VideoRect.left) || (m_rcVideoRectTest.top != m_VideoRect.top) || (m_rcVideoRectTest.right != m_VideoRect.right) || (m_rcVideoRectTest.bottom != m_VideoRect.bottom)) {
        m_rcVideoRectTest = m_VideoRect;
        m_upDX9ResizerTest = iDX9Resizer;
        m_xformTest = m_xform;
        if (m_pResizerPixelShaderX) {
            m_pResizerPixelShaderX = nullptr;
        }
        if (m_pResizerPixelShaderY) {
            m_pResizerPixelShaderY = nullptr;
            m_pIntermediateResizeSurface = nullptr;
            m_pIntermediateResizeTexture = nullptr;
        }

        // test if the input and output size for the resizers is equal, so nearest neighbor can be forced
        size_t upVRectWidth = m_VideoRect.right - m_VideoRect.left, upVRectHeight = m_VideoRect.bottom - m_VideoRect.top;
        m_bNoXresize = upVRectWidth == m_u32VideoWidth;
        m_bNoYresize = upVRectHeight == m_u32VideoHeight;
        m_upResizerN = (m_bNoXresize && m_bNoYresize) ? 0 : iDX9Resizer;

        // set up artifact clearing rectangles
        if (m_VideoRect.left > 0) {
            m_rcClearLeft.top = m_VideoRect.top;
            m_rcClearLeft.right = m_VideoRect.left;
            m_rcClearLeft.bottom = m_VideoRect.bottom;
        }
        if (m_VideoRect.top > 0) {
            m_rcClearTop.right = m_u32WindowWidth;
            m_rcClearTop.bottom =  m_VideoRect.top;
        }
        if (m_VideoRect.right < m_u32WindowWidth) {
            m_rcClearRight.left = m_VideoRect.right;
            m_rcClearRight.top = m_VideoRect.top;
            m_rcClearRight.right = m_u32WindowWidth;
            m_rcClearRight.bottom = m_VideoRect.bottom;
        }
        if (m_VideoRect.bottom < m_u32WindowHeight) {
            m_rcClearBottom.top = m_VideoRect.bottom;
            m_rcClearBottom.right = m_u32WindowWidth;
            m_rcClearBottom.bottom = m_u32WindowHeight;
        }

        char m_szVideoWidth[8], m_szVideoHeight[8], m_szWindowWidth[8];

        D3D_SHADER_MACRO macros[] = {
            {"Mw", m_szVideoWidth},
            {"Mh", m_szVideoHeight},
            {"Ms", m_bNoXresize ? m_szVideoWidth : m_szWindowWidth},
            {nullptr, nullptr}
        };

        // initialize hex strings partially
        unsigned __int8 u8Nibble;
        if (!m_bNoXresize) {
            *reinterpret_cast<__int16*>(m_szWindowWidth) = 'x0';
            m_szWindowWidth[7] = 0;

            // m_szWindowWidth; standard method for converting numbers to hex strings
            ASSERT(m_u32WindowWidth <= 0x9FFFF);// the method implementation limit here
            u8Nibble = static_cast<unsigned __int8>(m_u32WindowWidth >> 16); // each hexadecimal char stores 4 bits
            m_szWindowWidth[2] = '0' + u8Nibble;
            u8Nibble = (m_u32WindowWidth >> 12) & 15;
            u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
            m_szWindowWidth[3] = u8Nibble;
            u8Nibble = (m_u32WindowWidth >> 8) & 15;
            u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
            m_szWindowWidth[4] = u8Nibble;
            u8Nibble = (m_u32WindowWidth >> 4) & 15;
            u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
            m_szWindowWidth[5] = u8Nibble;
            u8Nibble = m_u32WindowWidth & 15;
            u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
            m_szWindowWidth[6] = u8Nibble;
        }

        *reinterpret_cast<__int16*>(m_szVideoWidth) = 'x0';
        *reinterpret_cast<__int16*>(m_szVideoHeight) = 'x0';

        m_szVideoWidth[7] = 0;
        m_szVideoHeight[7] = 0;

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
        ID3DBlob* pUtilBlob;// todo: move to secured Release() on failure status inside class
        if (FAILED(hr = m_fnD3DCompile(DSObjects::gk_aszResizerShader[m_upResizerN - 2], DSObjects::gk_au32LenResizerShader[m_upResizerN - 2], nullptr, macros, nullptr, (m_bNoXresize && (m_upResizerN > 2)) ? "mainY" : "mainX", m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &pUtilBlob, nullptr))) { return hr; }
        hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(pUtilBlob->GetBufferPointer()), &m_pResizerPixelShaderX);
        pUtilBlob->Release();
        if (FAILED(hr)) { return hr; }
        if ((m_upResizerN > 2) && !(m_bNoXresize || m_bNoYresize)) {
            // The list for resizers is offset by two; shaders 0 and 1 are never used
            if (FAILED(hr = m_fnD3DCompile(DSObjects::gk_aszResizerShader[m_upResizerN - 2], DSObjects::gk_au32LenResizerShader[m_upResizerN - 2], nullptr, macros, nullptr, "mainY", m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &pUtilBlob, nullptr))) { return hr; }
            hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(pUtilBlob->GetBufferPointer()), &m_pResizerPixelShaderY);
            pUtilBlob->Release();
            if (FAILED(hr)) { return hr; }
            if (FAILED(hr = m_pD3DDev->CreateTexture(m_u32WindowWidth, m_u32VideoHeight, 1, D3DUSAGE_RENDERTARGET, m_dfSurfaceType, D3DPOOL_DEFAULT, &m_pIntermediateResizeTexture, nullptr))) { return hr; }
            if (FAILED(hr = m_pIntermediateResizeTexture->GetSurfaceLevel(0, &m_pIntermediateResizeSurface))) { return hr; }
            if (FAILED(hr = m_pD3DDev->ColorFill(m_pIntermediateResizeSurface, nullptr, 0))) { return hr; }
        }
    }

    return S_OK;
}

HRESULT CBaseAP::TextureCopy(IDirect3DTexture9* pTexture)
{
    HRESULT hr;

    D3DSURFACE_DESC desc;
    if (!pTexture || FAILED(pTexture->GetLevelDesc(0, &desc))) {
        return E_FAIL;
    }

    float w = (float)desc.Width;
    float h = (float)desc.Height;
    MYD3DVERTEX<1> v[] = {
        {0, 0, 0.5f, 2.0f, 0, 0},
        {w, 0, 0.5f, 2.0f, 1, 0},
        {0, h, 0.5f, 2.0f, 0, 1},
        {w, h, 0.5f, 2.0f, 1, 1},
    };
    for (size_t i = 0; i < _countof(v); i++) {
        v[i].x -= 0.5;
        v[i].y -= 0.5;
    }
    hr = m_pD3DDev->SetTexture(0, pTexture);
    return TextureBlt(m_pD3DDev, v, D3DTEXF_POINT);
}

HRESULT CBaseAP::DrawRect(DWORD _Color, DWORD _Alpha, const CRect& _Rect)
{
    DWORD Color = D3DCOLOR_ARGB(_Alpha, GetRValue(_Color), GetGValue(_Color), GetBValue(_Color));
    MYD3DVERTEX<0> v[] = {
        {float(_Rect.left), float(_Rect.top), 0.5f, 2.0f, Color},
        {float(_Rect.right), float(_Rect.top), 0.5f, 2.0f, Color},
        {float(_Rect.left), float(_Rect.bottom), 0.5f, 2.0f, Color},
        {float(_Rect.right), float(_Rect.bottom), 0.5f, 2.0f, Color},
    };
    for (size_t i = 0; i < _countof(v); i++) {
        v[i].x -= 0.5;
        v[i].y -= 0.5;
    }
    return DrawRectBase(m_pD3DDev, v);
}

HRESULT CBaseAP::TextureResize(IDirect3DTexture9* pTexture, const Vector dst[4], const CRect& srcRect)
{
    HRESULT hr;

    float wf = m_fVideoWidth / (m_fWindowWidth + 1.0f), tx0 = -0.5f * wf, tx1 = (m_fWindowWidth + 0.5) * wf,
          hf = m_fVideoHeight / (m_fWindowHeight + 1.0f), ty0 = -0.5f * hf, ty1 = (m_fWindowHeight + 0.5) * hf;
    const MYD3DVERTEX<1> v[4] = {
        {dst[0].x - 0.5f, dst[0].y - 0.5f, dst[0].z, 1.0f / dst[0].z, tx0, ty0},
        {dst[1].x - 0.5f, dst[1].y - 0.5f, dst[1].z, 1.0f / dst[1].z, tx1, ty0},
        {dst[2].x - 0.5f, dst[2].y - 0.5f, dst[2].z, 1.0f / dst[2].z, tx0, ty1},
        {dst[3].x - 0.5f, dst[3].y - 0.5f, dst[3].z, 1.0f / dst[3].z, tx1, ty1}
    };

    hr = m_pD3DDev->SetPixelShader(m_pResizerPixelShaderX);
    hr = m_pD3DDev->SetTexture(0, pTexture);
    hr = TextureBlt(m_pD3DDev, v, D3DTEXF_POINT);
    return hr;
}

HRESULT CBaseAP::TextureResize2pass(IDirect3DTexture9* pTexture, const Vector dst[4], const CRect& srcRect)
{
    HRESULT hr;
    CComPtr<IDirect3DSurface9> pRTOld;
    hr = m_pD3DDev->GetRenderTarget(0, &pRTOld);
    hr = m_pD3DDev->SetRenderTarget(0, m_pIntermediateResizeSurface);
    hr = m_pD3DDev->Clear(0, nullptr, D3DCLEAR_TARGET, mk_pRendererSettings->dwBackgoundColor, 1.0f, 0);// clear it as soon as possible

    float swpor = 1.0f / (m_fWindowWidth + 1.0f), swph = m_fWindowWidth + 0.5f,
          wfi = m_fVideoWidth * swpor, txi0 = -0.5f * wfi, txi1 = swph * wfi,
          hfi = m_fVideoHeight / (m_fVideoHeight + 1.0f), tyi0 = -0.5f * hfi, tyi1 = (m_fVideoHeight + 0.5f) * hfi,
          oyi = m_fVideoHeight - 0.5f,
          wf = m_fWindowWidth * swpor, tx0 = -0.5f * wf, tx1 = swph * wf,
          hf = m_fVideoHeight / (m_fWindowHeight + 1.0), ty0 = -0.5f * hf, ty1 = (m_fWindowHeight + 0.5) * hf,
          ox = m_fWindowWidth - 0.5f;
    const MYD3DVERTEX<1> w[4] = {
        {dst[0].x - 0.5f, -0.5f,  0.5f, 2.0f, txi0, tyi0},
        {dst[1].x - 0.5f, -0.5f,  0.5f, 2.0f, txi1, tyi0},
        {dst[2].x - 0.5f, oyi,    0.5f, 2.0f, txi0, tyi1},
        {dst[3].x - 0.5f, oyi,    0.5f, 2.0f, txi1, tyi1}
    },
    v[4] = {
        { -0.5f, dst[0].y - 0.5f, dst[0].z, 1.0f / dst[0].z, tx0, ty0},
        {ox,    dst[1].y - 0.5f, dst[1].z, 1.0f / dst[1].z, tx1, ty0},
        { -0.5f, dst[2].y - 0.5f, dst[2].z, 1.0f / dst[2].z, tx0, ty1},
        {ox,    dst[3].y - 0.5f, dst[3].z, 1.0f / dst[3].z, tx1, ty1}
    };

    hr = m_pD3DDev->SetPixelShader(m_pResizerPixelShaderX);
    hr = m_pD3DDev->SetTexture(0, pTexture);
    hr = TextureBlt(m_pD3DDev, w, D3DTEXF_POINT);

    hr = m_pD3DDev->SetRenderTarget(0, pRTOld);
    hr = m_pD3DDev->SetPixelShader(m_pResizerPixelShaderY);
    hr = m_pD3DDev->SetTexture(0, m_pIntermediateResizeTexture);
    hr = TextureBlt(m_pD3DDev, v, D3DTEXF_POINT);
    return hr;
}

HRESULT CBaseAP::AlphaBlt(RECT* pSrc, RECT* pDst, IDirect3DTexture9* pTexture)
{
    CRect src(*pSrc), dst(*pDst);

    HRESULT hr;

    do {
        D3DSURFACE_DESC d3dsd;
        ZeroMemory(&d3dsd, sizeof(d3dsd));
        if (FAILED(pTexture->GetLevelDesc(0, &d3dsd)) /*|| d3dsd.Type != D3DRTYPE_TEXTURE*/) {
            break;
        }

        float w = (float)d3dsd.Width;
        float h = (float)d3dsd.Height;

        struct {
            float x, y, z, rhw;
            float tu, tv;
        }
        pVertices[] = {
            {(float)dst.left, (float)dst.top, 0.5f, 2.0f, (float)src.left / w, (float)src.top / h},
            {(float)dst.right, (float)dst.top, 0.5f, 2.0f, (float)src.right / w, (float)src.top / h},
            {(float)dst.left, (float)dst.bottom, 0.5f, 2.0f, (float)src.left / w, (float)src.bottom / h},
            {(float)dst.right, (float)dst.bottom, 0.5f, 2.0f, (float)src.right / w, (float)src.bottom / h},
        };

        for (size_t i = 0; i < _countof(pVertices); i++) {
            pVertices[i].x -= 0.5;
            pVertices[i].y -= 0.5;
        }

        hr = m_pD3DDev->SetTexture(0, pTexture);

        // GetRenderState fails for devices created with D3DCREATE_PUREDEVICE
        // so we need to provide default values in case GetRenderState fails
        DWORD abe, sb, db;
        if (FAILED(m_pD3DDev->GetRenderState(D3DRS_ALPHABLENDENABLE, &abe))) {
            abe = FALSE;
        }
        if (FAILED(m_pD3DDev->GetRenderState(D3DRS_SRCBLEND, &sb))) {
            sb = D3DBLEND_ONE;
        }
        if (FAILED(m_pD3DDev->GetRenderState(D3DRS_DESTBLEND, &db))) {
            db = D3DBLEND_ZERO;
        }

        hr = m_pD3DDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        hr = m_pD3DDev->SetRenderState(D3DRS_LIGHTING, FALSE);
        hr = m_pD3DDev->SetRenderState(D3DRS_ZENABLE, FALSE);
        hr = m_pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        hr = m_pD3DDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE); // pre-multiplied src and ...
        hr = m_pD3DDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCALPHA); // ... inverse alpha channel for dst

        hr = m_pD3DDev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        hr = m_pD3DDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        hr = m_pD3DDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

        hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

        hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

        hr = m_pD3DDev->SetPixelShader(nullptr);

        hr = m_pD3DDev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
        hr = m_pD3DDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, pVertices, sizeof(pVertices[0]));

        m_pD3DDev->SetTexture(0, nullptr);

        m_pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, abe);
        m_pD3DDev->SetRenderState(D3DRS_SRCBLEND, sb);
        m_pD3DDev->SetRenderState(D3DRS_DESTBLEND, db);

        return S_OK;
    } while (0);
    return E_FAIL;
}

// Update the array m_llJitter with a new vsync period. Calculate min, max and stddev.
void CBaseAP::SyncStats(LONGLONG syncTime)
{
    m_upNextJitter = (m_upNextJitter + 1) % NB_JITTER;
    LONGLONG jitter = syncTime - m_llLastSyncTime;
    m_llJitter[m_upNextJitter] = jitter;
    double syncDeviation = ((double)m_llJitter[m_upNextJitter] - m_dJitterMean) / 10000.0;
    if (abs(syncDeviation) > (GetDisplayCycle() / 2)) {
        m_upSyncGlitches++;
    }

    LONGLONG llJitterSum = 0;
    LONGLONG llJitterSumAvg = 0;
    for (size_t i = 0; i < NB_JITTER; ++i) {
        LONGLONG Jitter = m_llJitter[i];
        llJitterSum += Jitter;
        llJitterSumAvg += Jitter;
    }
    m_dJitterMean = double(llJitterSumAvg) / NB_JITTER;
    double DeviationSum = 0;

    for (size_t i = 0; i < NB_JITTER; ++i) {
        LONGLONG DevInt = m_llJitter[i] - m_dJitterMean;
        double Deviation = DevInt;
        DeviationSum += Deviation * Deviation;
        m_llMaxJitter = max(m_llMaxJitter, DevInt);
        m_llMinJitter = min(m_llMinJitter, DevInt);
    }

    m_dJitterStdDev = sqrt(DeviationSum / NB_JITTER);
    m_dAverageFrameRate = 10000000.0 / (double(llJitterSum) / NB_JITTER);
    m_llLastSyncTime = syncTime;
}

// Collect the difference between periodEnd and periodStart in an array, calculate mean and stddev.
void CBaseAP::SyncOffsetStats(LONGLONG syncOffset)
{
    m_upNextSyncOffset = (m_upNextSyncOffset + 1) & (NB_JITTER - 1); // modulo action by low bitmask
    m_llSyncOffset[m_upNextSyncOffset] = syncOffset;

    LONGLONG AvrageSum = 0;
    for (size_t i = 0; i < NB_JITTER; ++i) {
        LONGLONG Offset = m_llSyncOffset[i];
        AvrageSum += Offset;
        m_llMaxSyncOffset = max(m_llMaxSyncOffset, Offset);
        m_llMinSyncOffset = min(m_llMinSyncOffset, Offset);
    }
    double MeanOffset = double(AvrageSum) / NB_JITTER;
    double DeviationSum = 0;
    for (size_t i = 0; i < NB_JITTER; ++i) {
        double Deviation = double(m_llSyncOffset[i]) - MeanOffset;
        DeviationSum += Deviation * Deviation;
    }
    double StdDev = sqrt(DeviationSum / NB_JITTER);

    m_dSyncOffsetAvr = MeanOffset;
    m_dSyncOffsetStdDev = StdDev;
}

void CBaseAP::UpdateAlphaBitmap()
{
    m_avVMR9AlphaBitmapData.Free();

    if (!(m_abVMR9AlphaBitmap.dwFlags & VMRBITMAP_DISABLE)) {
        HBITMAP hBitmap = (HBITMAP)GetCurrentObject(m_abVMR9AlphaBitmap.hdc, OBJ_BITMAP);
        if (!hBitmap) {
            return;
        }
        DIBSECTION info = {0};
        if (!::GetObject(hBitmap, sizeof(DIBSECTION), &info)) {
            return;
        }

        m_lVMR9AlphaBitmapWidthBytes = info.dsBm.bmWidthBytes;

        if (m_avVMR9AlphaBitmapData.Allocate(info.dsBm.bmWidthBytes * info.dsBm.bmHeight)) {
            memcpy((BYTE*)m_avVMR9AlphaBitmapData, info.dsBm.bmBits, info.dsBm.bmWidthBytes * info.dsBm.bmHeight);
        }
    }
}

// Present a sample (frame) using DirectX.
__declspec(nothrow noalias) void CBaseAP::Paint(__in bool fAll)
{
    if (m_bPendingResetDevice) {
        SendResetRequest();
        return;
    }

    CRenderersData* pRenderersData = GetRenderersData();
    D3DRASTER_STATUS rasterStatus;
    REFERENCE_TIME llCurRefTime = 0;
    REFERENCE_TIME llSyncOffset = 0;
    double dSyncOffset = 0.0;

    CAutoLock cRenderLock(&m_csAllocatorLock);

    // Estimate time for next vblank based on number of remaining lines in this frame. This algorithm seems to be
    // accurate within one ms why there should not be any need for a more accurate one. The wiggly line seen
    // when using sync to nearest and sync display is most likely due to inaccuracies in the audio-card-based
    // reference clock. The wiggles are not seen with the perfcounter-based reference clock of the sync to video option.
    m_pD3DDev->GetRasterStatus(0, &rasterStatus);
    m_uScanLineEnteringPaint = rasterStatus.ScanLine;
    if (m_pRefClock) {
        m_pRefClock->GetTime(&llCurRefTime);
    }
    int dScanLines = max((int)m_ScreenSize.cy - m_uScanLineEnteringPaint, 0);
    dSyncOffset = dScanLines * m_dDetectedScanlineTime; // ms
    llSyncOffset = REFERENCE_TIME(10000.0 * dSyncOffset); // Reference time units (100 ns)
    m_llEstVBlankTime = llCurRefTime + llSyncOffset; // Estimated time for the start of next vblank

    if (!m_u32VideoWidth || !m_u32VideoHeight || !m_apVideoSurface) { return; }

    HRESULT hr;
    CRect rSrcVid(0, 0, m_u32VideoWidth, m_u32VideoHeight);
    CRect rDstVid(m_VideoRect);
    CRect rSrcPri(0, 0, m_VideoRect.right - m_VideoRect.left, m_VideoRect.bottom - m_VideoRect.top);
    CRect rDstPri(0, 0, m_u32WindowWidth, m_u32WindowHeight);

    m_pD3DDev->BeginScene();
    CComPtr<IDirect3DSurface9> pBackBuffer;
    m_pD3DDev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
    m_pD3DDev->SetRenderTarget(0, pBackBuffer);
    hr = m_pD3DDev->Clear(0, nullptr, D3DCLEAR_TARGET, mk_pRendererSettings->dwBackgoundColor, 1.0f, 0);
    if (!rDstVid.IsRectEmpty()) {
        if (m_apVideoTexture[m_nCurSurface]) {
            CComPtr<IDirect3DTexture9> pVideoTexture = m_apVideoTexture[m_nCurSurface];

            if (m_apVideoTexture[m_nDXSurface] && m_apVideoTexture[m_nDXSurface + 1] && !m_apCustomPixelShaders[0].IsEmpty()) {
                static __int64 counter = 0;
                static long start = clock();

                long stop = clock();
                long diff = stop - start;

                if (diff >= 10 * 60 * CLOCKS_PER_SEC) {
                    start = stop;    // reset after 10 min (ps float has its limits in both range and accuracy)
                }

                int src = m_nCurSurface, dst = m_nDXSurface;

                D3DSURFACE_DESC desc;
                m_apVideoTexture[src]->GetLevelDesc(0, &desc);

                float fConstData[][4] = {
                    {(float)desc.Width, (float)desc.Height, (float)(counter++), (float)diff / CLOCKS_PER_SEC},
                    {1.0f / desc.Width, 1.0f / desc.Height, 0, 0},
                };

                hr = m_pD3DDev->SetPixelShaderConstantF(0, (float*)fConstData, _countof(fConstData));

                CComPtr<IDirect3DSurface9> pRT;
                hr = m_pD3DDev->GetRenderTarget(0, &pRT);

                POSITION pos = m_apCustomPixelShaders[0].GetHeadPosition();
                while (pos) {
                    pVideoTexture = m_apVideoTexture[dst];

                    hr = m_pD3DDev->SetRenderTarget(0, m_apVideoSurface[dst]);

                    EXTERNALSHADER& ceShader = m_apCustomPixelShaders[0].GetNext(pos);
                    if (!ceShader.pPixelShader && FAILED(hr = CompileShader(ceShader.strSrcData, ceShader.uiSrcLen, "main", &ceShader.pPixelShader))) {
                        pos = m_apCustomPixelShaders[0].GetHeadPosition();
                        while (pos) {
                            EXTERNALSHADER& ceShaderL = m_apCustomPixelShaders[0].GetNext(pos);
                            if (ceShaderL.pPixelShader) { ceShaderL.pPixelShader->Release(); }
                            delete[] ceShaderL.strSrcData;
                        }
                        m_apCustomPixelShaders[0].RemoveAll();
                        break;
                    }
                    hr = m_pD3DDev->SetPixelShader(ceShader.pPixelShader);
                    TextureCopy(m_apVideoTexture[src]);

                    src     = dst;
                    if (++dst >= m_nDXSurface + 2) {
                        dst = m_nDXSurface;
                    }
                }

                hr = m_pD3DDev->SetRenderTarget(0, pRT);
                hr = m_pD3DDev->SetPixelShader(nullptr);
            }

            LONG w = m_VideoRect.right - m_VideoRect.left, h = m_VideoRect.bottom - m_VideoRect.top;
            double ldiv = 1.0 / (sqrt(static_cast<double>(w * w + h * h)) * 1.5 + 1.0),
                   vrl = static_cast<double>(m_VideoRect.left), vrt = static_cast<double>(m_VideoRect.top), vrr = static_cast<double>(m_VideoRect.right), vrb = static_cast<double>(m_VideoRect.bottom);
            Vector dst[4] = {
                Vector(vrl, vrt, 0.0),
                Vector(vrr, vrt, 0.0),
                Vector(vrl, vrb, 0.0),
                Vector(vrr, vrb, 0.0)
            };
            Vector center((vrl + vrr) * 0.5, (vrt + vrb) * 0.5, 0.0);

            ptrdiff_t i = 3;
            do {
                dst[i] = m_xform << (dst[i] - center);
                dst[i].z = dst[i].z * ldiv + 0.5;
                double zdiv = 0.5 / dst[i].z;
                dst[i].x *= zdiv;
                dst[i].y *= zdiv;
                dst[i] += center;
            } while (--i >= 0);

            bool bScreenSpacePixelShaders = !m_apCustomPixelShaders[1].IsEmpty();

            hr = InitResizers(mk_pRendererSettings->iDX9Resizer);

            if (!m_pScreenSizeTemporaryTexture[0] || !m_pScreenSizeTemporaryTexture[1]) {
                bScreenSpacePixelShaders = false;
            }

            if (bScreenSpacePixelShaders) {
                CComPtr<IDirect3DSurface9> pRT;
                hr = m_pScreenSizeTemporaryTexture[1]->GetSurfaceLevel(0, &pRT);
                if (hr != S_OK) {
                    bScreenSpacePixelShaders = false;
                }
                if (bScreenSpacePixelShaders) {
                    hr = m_pD3DDev->SetRenderTarget(0, pRT);
                    if (hr != S_OK) {
                        bScreenSpacePixelShaders = false;
                    }
                    hr = m_pD3DDev->Clear(0, nullptr, D3DCLEAR_TARGET, mk_pRendererSettings->dwBackgoundColor, 1.0f, 0);
                }
            }

            // resizer section
            if (m_upResizerN < 3) { hr = TextureResize(pVideoTexture, dst, rSrcVid); }
            else { hr = TextureResize2pass(pVideoTexture, dst, rSrcVid); }

            if (bScreenSpacePixelShaders) {
                static __int64 counter = 555;
                static long start = clock() + 333;

                long stop = clock() + 333;
                long diff = stop - start;

                if (diff >= 10 * 60 * CLOCKS_PER_SEC) {
                    start = stop;    // reset after 10 min (ps float has its limits in both range and accuracy)
                }

                D3DSURFACE_DESC desc;
                m_pScreenSizeTemporaryTexture[0]->GetLevelDesc(0, &desc);

                float fConstData[][4] = {
                    {(float)desc.Width, (float)desc.Height, (float)(counter++), (float)diff / CLOCKS_PER_SEC},
                    {1.0f / desc.Width, 1.0f / desc.Height, 0, 0},
                };

                hr = m_pD3DDev->SetPixelShaderConstantF(0, (float*)fConstData, _countof(fConstData));

                int src = 1, dest = 0;

                POSITION pos = m_apCustomPixelShaders[1].GetHeadPosition();
                while (pos) {
                    if (m_apCustomPixelShaders[1].GetTailPosition() == pos) {
                        m_pD3DDev->SetRenderTarget(0, pBackBuffer);
                    } else {
                        CComPtr<IDirect3DSurface9> pRT;
                        hr = m_pScreenSizeTemporaryTexture[dest]->GetSurfaceLevel(0, &pRT);
                        m_pD3DDev->SetRenderTarget(0, pRT);
                    }

                    EXTERNALSHADER& ceShader = m_apCustomPixelShaders[1].GetNext(pos);
                    if (!ceShader.pPixelShader && FAILED(hr = CompileShader(ceShader.strSrcData, ceShader.uiSrcLen, "main", &ceShader.pPixelShader))) {
                        pos = m_apCustomPixelShaders[1].GetHeadPosition();
                        while (pos) {
                            EXTERNALSHADER& ceShaderL = m_apCustomPixelShaders[1].GetNext(pos);
                            if (ceShaderL.pPixelShader) { ceShaderL.pPixelShader->Release(); }
                            delete[] ceShaderL.strSrcData;
                        }
                        m_apCustomPixelShaders[1].RemoveAll();
                        break;
                    }
                    hr = m_pD3DDev->SetPixelShader(ceShader.pPixelShader);
                    TextureCopy(m_pScreenSizeTemporaryTexture[src]);

                    std::swap(src, dest);
                }

                hr = m_pD3DDev->SetPixelShader(nullptr);
            }
        } else { ASSERT(0); }// code planned for removal
    }

    // subtitles
    if (CBSubPic* pSubPic = m_pSubPicQueue->LookupSubPic(m_i64Now)) {
        RECT arcSourceDest[2];
        if (pSubPic->GetSourceAndDest(arcSourceDest)) {
            CDX9SubPic* pDX9SubPic = static_cast<CDX9SubPic*>(pSubPic);

            if (FAILED(hr = m_pD3DDev->SetPixelShader(nullptr))
                    || FAILED(hr = m_pD3DDev->SetTexture(0, pDX9SubPic->m_pTexture))) { goto EndSubtitleBlend; }

            bool resized = (arcSourceDest->right - arcSourceDest->left != arcSourceDest[1].right - arcSourceDest[1].left) || (arcSourceDest->bottom - arcSourceDest->top != arcSourceDest[1].bottom - arcSourceDest[1].top);
            if (resized) {
                m_pD3DDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                m_pD3DDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            } else {
                // remove this part once the sampler states are properly handled in the renderer
                m_pD3DDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
                m_pD3DDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            }

            m_pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

            // todo: make these settings default in createdevice
            m_pD3DDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);// pre-multiplied src
            m_pD3DDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCALPHA);// inverse alpha channel for dst
            m_pD3DDev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            m_pD3DDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            m_pD3DDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

            float rdl = static_cast<float>(arcSourceDest[1].left + m_i32SubWindowOffsetLeft), rdr = static_cast<float>(arcSourceDest[1].right + m_i32SubWindowOffsetLeft), rdt = static_cast<float>(arcSourceDest[1].top + m_i32SubWindowOffsetTop), rdb = static_cast<float>(arcSourceDest[1].bottom + m_i32SubWindowOffsetTop);
            if (mk_pRendererSettings->nSPCMaxRes == 1) { // adapt three-quarter-sized subtitle texture
                rdl *= 1.0f / 0.75f;
                rdt *= 1.0f / 0.75f;
                rdr *= 1.0f / 0.75f;
                rdb *= 1.0f / 0.75f;
            } else if (mk_pRendererSettings->nSPCMaxRes > 1) { // adapt half-sized subtitle texture
                rdl *= 2.0f;
                rdt *= 2.0f;
                rdr *= 2.0f;
                rdb *= 2.0f;
            }
            rdl += -0.5f;
            rdt += -0.5f;
            rdr += -0.5f;
            rdb += -0.5f;

            __declspec(align(16)) CUSTOMVERTEX_TEX1 v[4] = {
                {rdl, rdt, 0.5f, 2.0f, 0.0f, 0.0f},
                {rdr, rdt, 0.5f, 2.0f, 1.0f, 0.0f},
                {rdl, rdb, 0.5f, 2.0f, 0.0f, 1.0f},
                {rdr, rdb, 0.5f, 2.0f, 1.0f, 1.0f}
            };

            m_pD3DDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(CUSTOMVERTEX_TEX1));
            m_pD3DDev->EndScene();

            m_pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

            if (resized) {
                m_pD3DDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
                m_pD3DDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            }
        }
EndSubtitleBlend:
        pSubPic->Release();
    }

    if (m_abVMR9AlphaBitmap.dwFlags & VMRBITMAP_UPDATE) {
        CAutoLock BitMapLock(&m_csVMR9AlphaBitmapLock);
        CRect       rcSrc(m_abVMR9AlphaBitmap.rSrc);
        m_pOSDTexture   = nullptr;
        m_pOSDSurface   = nullptr;
        if (!(m_abVMR9AlphaBitmap.dwFlags & VMRBITMAP_DISABLE) && (BYTE*)m_avVMR9AlphaBitmapData) {
            if ((m_pD3DXLoadSurfaceFromMemory != nullptr) &&
                    SUCCEEDED(hr = m_pD3DDev->CreateTexture(rcSrc.Width(), rcSrc.Height(), 1,
                                   D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                   D3DPOOL_DEFAULT, &m_pOSDTexture, nullptr))) {
                if (SUCCEEDED(hr = m_pOSDTexture->GetSurfaceLevel(0, &m_pOSDSurface))) {
                    hr = m_pD3DXLoadSurfaceFromMemory(m_pOSDSurface, nullptr, nullptr, (BYTE*)m_avVMR9AlphaBitmapData, D3DFMT_A8R8G8B8, m_lVMR9AlphaBitmapWidthBytes,
                                                      nullptr, nullptr, D3DX_FILTER_NONE, m_abVMR9AlphaBitmap.clrSrcKey);
                }
                if (FAILED(hr)) {
                    m_pOSDTexture   = nullptr;
                    m_pOSDSurface   = nullptr;
                }
            }
        }
        m_abVMR9AlphaBitmap.dwFlags ^= VMRBITMAP_UPDATE;
    }
    if (pRenderersData->m_fDisplayStats) {
        DrawStats();
    }
    if (m_pOSDTexture) {
        AlphaBlt(rSrcPri, rDstPri, m_pOSDTexture);
    }
    m_pD3DDev->EndScene();

    if (m_pD3DDevEx) {
        if (m_upIsFullscreen) {
            hr = m_pD3DDevEx->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
        } else {
            hr = m_pD3DDevEx->PresentEx(rSrcPri, rDstPri, nullptr, nullptr, 0);
        }
    } else {
        if (m_upIsFullscreen) {
            hr = m_pD3DDev->Present(nullptr, nullptr, nullptr, nullptr);
        } else {
            hr = m_pD3DDev->Present(rSrcPri, rDstPri, nullptr, nullptr);
        }
    }
    if (FAILED(hr)) {
        DEBUG_ONLY(_tprintf_s(_T("Device lost or something\n")));
    }
    // Calculate timing statistics
    if (m_pRefClock) {
        m_pRefClock->GetTime(&llCurRefTime);    // To check if we called Present too late to hit the right vsync
    }
    m_llEstVBlankTime = max(m_llEstVBlankTime, llCurRefTime); // Sometimes the real value is larger than the estimated value (but never smaller)
    if (pRenderersData->m_fDisplayStats < 3) { // Partial on-screen statistics
        SyncStats(m_llEstVBlankTime);    // Max of estimate and real. Sometimes Present may actually return immediately so we need the estimate as a lower bound
    }
    if (pRenderersData->m_fDisplayStats == 1) { // Full on-screen statistics
        SyncOffsetStats(-llSyncOffset);    // Minus because we want time to flow downward in the graph in DrawStats
    }

    // Adjust sync
    double frameCycle = (double)((m_llSampleTime - m_llLastSampleTime) / 10000.0);
    if (frameCycle < 0) {
        frameCycle = 0.0;    // Happens when searching.
    }

    if (mk_pRendererSettings->bSynchronizeVideo) {
        m_pGenlock->ControlClock(dSyncOffset, frameCycle);
    } else if (mk_pRendererSettings->bSynchronizeDisplay) {
        m_pGenlock->ControlDisplay(dSyncOffset, frameCycle);
    } else {
        m_pGenlock->UpdateStats(dSyncOffset, frameCycle);    // No sync or sync to nearest neighbor
    }

    m_dFrameCycle = m_pGenlock->frameCycleAvg;
    if (m_dFrameCycle > 0.0) {
        m_dDetectedVideoFrameRate = 1000.0 / m_dFrameCycle;
    }
    m_dCycleDifference = GetCycleDifference();
    if (abs(m_dCycleDifference) < 0.05) { // If less than 5% speed difference
        m_bSnapToVSync = true;
    } else {
        m_bSnapToVSync = false;
    }

    // Check how well audio is matching rate (if at all)
    DWORD tmp;
    if (m_pAudioStats != nullptr) {
        m_pAudioStats->GetStatParam(AM_AUDREND_STAT_PARAM_SLAVE_ACCUMERROR, &m_lAudioLag, &tmp);
        m_lAudioLagMin = min((long)m_lAudioLag, m_lAudioLagMin);
        m_lAudioLagMax = max((long)m_lAudioLag, m_lAudioLagMax);
        m_pAudioStats->GetStatParam(AM_AUDREND_STAT_PARAM_SLAVE_MODE, &m_lAudioSlaveMode, &tmp);
    }

    if (pRenderersData->m_bResetStats) {
        pRenderersData->m_bResetStats = false;
        ResetStats();
    }

    bool fResetDevice = m_bPendingResetDevice;
    if (hr == D3DERR_DEVICELOST && m_pD3DDev->TestCooperativeLevel() == D3DERR_DEVICENOTRESET || hr == S_PRESENT_MODE_CHANGED) {
        fResetDevice = true;
    }
    if (SettingsNeedResetDevice()) {
        fResetDevice = true;
    }

    if (m_pDwmIsCompositionEnabled) {
        BOOL boCompositionEnabled;
        m_pDwmIsCompositionEnabled(&boCompositionEnabled);
        if (m_boCompositionEnabled != boCompositionEnabled) {
            if (m_upIsFullscreen) { m_boCompositionEnabled = boCompositionEnabled; }
            else { fResetDevice = true; }
        }
    }

    if (!mk_pRendererSettings->bD3DFullscreen) {
        LONGLONG time = PerfCounter100ns();
        if (time > m_llLastAdapterCheck + 20000000) { // check every 2 sec.
            m_llLastAdapterCheck = time;
#ifdef _DEBUG
            D3DDEVICE_CREATION_PARAMETERS Parameters;
            if (SUCCEEDED(m_pD3DDev->GetCreationParameters(&Parameters))) {
                ASSERT(Parameters.AdapterOrdinal == m_uiCurrentAdapter);
            }
#endif
            if (m_uiCurrentAdapter != GetAdapter(m_pD3D)) {
                fResetDevice = true;
            }
#ifdef _DEBUG
            else {
                ASSERT(m_pD3D->GetAdapterMonitor(m_uiCurrentAdapter) == m_pD3D->GetAdapterMonitor(GetAdapter(m_pD3D)));
            }
#endif
        }
    }

    if (fResetDevice) {
        m_bPendingResetDevice = true;
        SendResetRequest();
    }
}

void CBaseAP::SendResetRequest()
{
    if (!m_bDeviceResetRequested) {
        m_bDeviceResetRequested = true;
        AfxGetApp()->m_pMainWnd->PostMessage(WM_RESET_DEVICE);
    }
}

void CBaseAP::ResetLocalDevice()
{
    DeleteSurfaces();
    HRESULT hr;
    CString Error;
    if (FAILED(hr = CreateDXDevice(Error)) || FAILED(hr = AllocSurfaces())) {
        m_bDeviceResetRequested = false;
        return;
    }
    m_pGenlock->SetMonitor(GetAdapter(m_pD3D));
    m_pGenlock->GetTiming();
    OnResetDevice();
    m_bDeviceResetRequested = false;
    m_bPendingResetDevice = false;
    return;
}

void CBaseAP::DrawText(const RECT& rc, const CString& strText, int _Priority)
{
    if (_Priority < 1) {
        return;
    }
    int Quality = 1;
    D3DXCOLOR Color1(1.0f, 0.2f, 0.2f, 1.0f);
    D3DXCOLOR Color0(0.0f, 0.0f, 0.0f, 1.0f);
    RECT Rect1 = rc;
    RECT Rect2 = rc;
    if (Quality == 1) {
        OffsetRect(&Rect2 , 2, 2);
    } else {
        OffsetRect(&Rect2 , -1, -1);
    }
    if (Quality > 0) {
        m_pFont->DrawText(m_pSprite, strText, -1, &Rect2, DT_NOCLIP, Color0);
    }
    OffsetRect(&Rect2 , 1, 0);
    if (Quality > 3) {
        m_pFont->DrawText(m_pSprite, strText, -1, &Rect2, DT_NOCLIP, Color0);
    }
    OffsetRect(&Rect2 , 1, 0);
    if (Quality > 2) {
        m_pFont->DrawText(m_pSprite, strText, -1, &Rect2, DT_NOCLIP, Color0);
    }
    OffsetRect(&Rect2 , 0, 1);
    if (Quality > 3) {
        m_pFont->DrawText(m_pSprite, strText, -1, &Rect2, DT_NOCLIP, Color0);
    }
    OffsetRect(&Rect2 , 0, 1);
    if (Quality > 1) {
        m_pFont->DrawText(m_pSprite, strText, -1, &Rect2, DT_NOCLIP, Color0);
    }
    OffsetRect(&Rect2 , -1, 0);
    if (Quality > 3) {
        m_pFont->DrawText(m_pSprite, strText, -1, &Rect2, DT_NOCLIP, Color0);
    }
    OffsetRect(&Rect2 , -1, 0);
    if (Quality > 2) {
        m_pFont->DrawText(m_pSprite, strText, -1, &Rect2, DT_NOCLIP, Color0);
    }
    OffsetRect(&Rect2 , 0, -1);
    if (Quality > 3) {
        m_pFont->DrawText(m_pSprite, strText, -1, &Rect2, DT_NOCLIP, Color0);
    }
    m_pFont->DrawText(m_pSprite, strText, -1, &Rect1, DT_NOCLIP, Color1);
}

void CBaseAP::DrawStats()
{
    CRenderersData* pRenderersData = GetRenderersData();

    LONGLONG llMaxJitter = m_llMaxJitter;
    LONGLONG llMinJitter = m_llMinJitter;

    RECT rc = {20, 20, 520, 520 };
    // pRenderersData->m_fDisplayStats = 1 for full stats, 2 for little less, 3 for basic, 0 for no stats
    if (m_pFont && m_pSprite) {
        m_pSprite->Begin(D3DXSPRITE_ALPHABLEND);
        CString strText;
        int TextHeight = 25.0 * m_TextScale + 0.5;

        strText.Format(L"Frames drawn from stream start: %d | Sample time stamp: %d ms", m_pcFramesDrawn, (LONG)(m_llSampleTime / 10000));
        DrawText(rc, strText, 1);
        OffsetRect(&rc, 0, TextHeight);

        if (pRenderersData->m_fDisplayStats == 1) {
            strText.Format(L"Frame cycle  : %.3f ms [%.3f ms, %.3f ms]  Actual  %+5.3f ms [%+.3f ms, %+.3f ms]", m_dFrameCycle, m_pGenlock->minFrameCycle, m_pGenlock->maxFrameCycle, m_dJitterMean / 10000.0, (double(llMinJitter) / 10000.0), (double(llMaxJitter) / 10000.0));
            DrawText(rc, strText, 1);
            OffsetRect(&rc, 0, TextHeight);

            strText.Format(L"Display cycle: Measured closest match %.3f ms   Measured base %.3f ms", m_dOptimumDisplayCycle, m_dEstRefreshCycle);
            DrawText(rc, strText, 1);
            OffsetRect(&rc, 0, TextHeight);

            strText.Format(L"Frame rate   : %.3f fps   Actual frame rate: %.3f fps", m_dDetectedVideoFrameRate, 10000000.0 / m_dJitterMean);
            DrawText(rc, strText, 1);
            OffsetRect(&rc, 0, TextHeight);

            strText.Format(L"Windows      : Display cycle %.3f ms    Display refresh rate %.3f Hz", m_dD3DRefreshCycle, m_dD3DRefreshRate);
            DrawText(rc, strText, 1);
            OffsetRect(&rc, 0, TextHeight);

            if (m_pGenlock->powerstripTimingExists) {
                strText.Format(L"Powerstrip   : Display cycle %.3f ms    Display refresh rate %.3f Hz", 1000.0 / m_pGenlock->curDisplayFreq, m_pGenlock->curDisplayFreq);
                DrawText(rc, strText, 1);
                OffsetRect(&rc, 0, TextHeight);
            }

            if (!(m_dcCaps.Caps & D3DCAPS_READ_SCANLINE)) {
                strText = L"Scan line err: Graphics device does not support scan line access. No sync is possible";
                DrawText(rc, strText, 1);
                OffsetRect(&rc, 0, TextHeight);
            }

#ifdef _DEBUG
            if (m_pD3DDevEx) {
                CComPtr<IDirect3DSwapChain9> pSC;
                HRESULT hr = m_pD3DDevEx->GetSwapChain(0, &pSC);
                CComQIPtr<IDirect3DSwapChain9Ex> pSCEx = pSC;
                if (pSCEx) {
                    D3DPRESENTSTATS stats;
                    hr = pSCEx->GetPresentStats(&stats);
                    if (SUCCEEDED(hr)) {
                        strText = L"Graphics device present stats:";
                        DrawText(rc, strText, 1);
                        OffsetRect(&rc, 0, TextHeight);

                        strText.Format(L"    PresentCount %d PresentRefreshCount %d SyncRefreshCount %d",
                                       stats.PresentCount, stats.PresentRefreshCount, stats.SyncRefreshCount);
                        DrawText(rc, strText, 1);
                        OffsetRect(&rc, 0, TextHeight);

                        LARGE_INTEGER Freq;
                        QueryPerformanceFrequency(&Freq);
                        Freq.QuadPart /= 1000;
                        strText.Format(L"    SyncQPCTime %dms SyncGPUTime %dms",
                                       stats.SyncQPCTime.QuadPart / Freq.QuadPart, stats.SyncGPUTime.QuadPart / Freq.QuadPart);
                        DrawText(rc, strText, 1);
                        OffsetRect(&rc, 0, TextHeight);
                    } else {
                        strText = L"Graphics device does not support present stats";
                        DrawText(rc, strText, 1);
                        OffsetRect(&rc, 0, TextHeight);
                    }
                }
            }
#endif

            strText.Format(L"Video size   : %u × %u  (AR = %u : %u)  Display resolution %u × %u ", m_u32VideoWidth, m_u32VideoHeight, m_u32AspectRatioWidth, m_u32AspectRatioHeight, m_ScreenSize.cx, m_ScreenSize.cy);
            DrawText(rc, strText, 1);
            OffsetRect(&rc, 0, TextHeight);

            if (mk_pRendererSettings->bSynchronizeDisplay || mk_pRendererSettings->bSynchronizeVideo) {
                if (mk_pRendererSettings->bSynchronizeDisplay && !m_pGenlock->PowerstripRunning()) {
                    strText = L"Sync error   : PowerStrip is not running. No display sync is possible.";
                    DrawText(rc, strText, 1);
                    OffsetRect(&rc, 0, TextHeight);
                } else {
                    strText.Format(L"Sync adjust  : %d | # of adjustments: %d", m_pGenlock->adjDelta, (m_pGenlock->clockAdjustmentsMade + m_pGenlock->displayAdjustmentsMade) / 2);
                    DrawText(rc, strText, 1);
                    OffsetRect(&rc, 0, TextHeight);
                }
            }
        }

        strText.Format(L"Sync offset  : Average %3.1f ms [%.1f ms, %.1f ms]   Target %3.1f ms", m_pGenlock->syncOffsetAvg, m_pGenlock->minSyncOffset, m_pGenlock->maxSyncOffset, mk_pRendererSettings->fTargetSyncOffset);
        DrawText(rc, strText, 1);
        OffsetRect(&rc, 0, TextHeight);

        strText.Format(L"Sync status  : glitches %d,  display-frame cycle mismatch: %7.3f %%,  dropped frames %d", m_upSyncGlitches, 100 * m_dCycleDifference, m_pcFramesDropped);
        DrawText(rc, strText, 1);
        OffsetRect(&rc, 0, TextHeight);

        if (pRenderersData->m_fDisplayStats == 1) {
            if (m_pAudioStats && mk_pRendererSettings->bSynchronizeVideo) {
                strText.Format(L"Audio lag   : %3d ms [%d ms, %d ms] | %s", m_lAudioLag, m_lAudioLagMin, m_lAudioLagMax, (m_lAudioSlaveMode == 4) ? _T("Audio renderer is matching rate (for analog sound output)") : _T("Audio renderer is not matching rate"));
                DrawText(rc, strText, 1);
                OffsetRect(&rc, 0, TextHeight);
            }

            strText.Format(L"Sample time  : waiting %3d ms", m_lNextSampleWait);
            if (mk_pRendererSettings->bSynchronizeNearest) {
                CString temp;
                temp.Format(L"  paint time correction: %3d ms  Hysteresis: %d", m_lShiftToNearest, m_llHysteresis / 10000);
                strText += temp;
            }
            DrawText(rc, strText, 1);
            OffsetRect(&rc, 0, TextHeight);

            strText.Format(L"Buffering    : Buffered %3d    Free %3d    Current Surface %3d", m_nUsedBuffer, m_nDXSurface - m_nUsedBuffer, m_nCurSurface, m_nVMR9Surfaces, m_iVMR9Surface);
            DrawText(rc, strText, 1);
            OffsetRect(&rc, 0, TextHeight);

            strText = L"Settings     : ";

            if (m_upIsFullscreen) {
                strText += "D3DFS ";
            }
            if (mk_pRendererSettings->iVMRDisableDesktopComposition) {
                strText += "DisDC ";
            }
            if (mk_pRendererSettings->bSynchronizeVideo) {
                strText += "SyncVideo ";
            }
            if (mk_pRendererSettings->bSynchronizeDisplay) {
                strText += "SyncDisplay ";
            }
            if (mk_pRendererSettings->bSynchronizeNearest) {
                strText += "SyncNearest ";
            }
            if (m_upHighColorResolution) {
                strText += "10 bit ";
            }

            DrawText(rc, strText, 1);
            OffsetRect(&rc, 0, TextHeight);

            strText.Format(L"%-13s: ", GetDXVAVersion());
            size_t upStringLength;
            LPCTSTR szDXVAdd = GetDXVADecoderDescription(&upStringLength);
            strText.Append(szDXVAdd, upStringLength);
            DrawText(rc, strText, 1);
            OffsetRect(&rc, 0, TextHeight);

            strText.Format(L"DirectX SDK  : %hu", m_u16DXSdkRelease);
            DrawText(rc, strText, 1);
            OffsetRect(&rc, 0, TextHeight);

            for (size_t i = 0; i < 6; ++i) {
                if (m_strStatsMsg[i][0]) {
                    DrawText(rc, m_strStatsMsg[i], 1);
                    OffsetRect(&rc, 0, TextHeight);
                }
            }
        }
        OffsetRect(&rc, 0, TextHeight); // Extra "line feed"
        m_pSprite->End();
    }

    if (m_pLine && (pRenderersData->m_fDisplayStats < 3)) {
        D3DXVECTOR2 Points[NB_JITTER];
        size_t nIndex;

        size_t DrawWidth = 625;
        size_t DrawHeight = 250;
        size_t Alpha = 80;
        size_t StartX = m_u32WindowWidth - (DrawWidth + 20);
        size_t StartY = m_u32WindowHeight - (DrawHeight + 20);

        DrawRect(RGB(0, 0, 0), Alpha, CRect(StartX, StartY, StartX + DrawWidth, StartY + DrawHeight));
        m_pLine->SetWidth(2.5);
        m_pLine->SetAntialias(1);
        m_pLine->Begin();

        for (size_t i = 0; i <= DrawHeight; i += 5) {
            Points[0].x = (float)StartX;
            Points[0].y = (float)(StartY + i);
            Points[1].x = (float)(StartX + (((i + 25) % 25) ? 50 : 625));
            Points[1].y = (float)(StartY + i);
            m_pLine->Draw(Points, 2, D3DCOLOR_XRGB(100, 100, 255));
        }

        for (size_t i = 0; i < DrawWidth; i += 125) { // Every 25:th sample
            Points[0].x = (float)(StartX + i);
            Points[0].y = (float)(StartY + DrawHeight / 2);
            Points[1].x = (float)(StartX + i);
            Points[1].y = (float)(StartY + DrawHeight / 2 + 10);
            m_pLine->Draw(Points, 2, D3DCOLOR_XRGB(100, 100, 255));
        }

        for (size_t i = 0; i < NB_JITTER; i++) {
            nIndex = (m_upNextJitter + 1 + i) % NB_JITTER;
            double Jitter = m_llJitter[nIndex] - m_dJitterMean;
            Points[i].x  = (float)(StartX + (i * 5));
            Points[i].y  = (float)(StartY + (Jitter / 2000.0 + 125.0));
        }
        m_pLine->Draw(Points, NB_JITTER, D3DCOLOR_XRGB(255, 100, 100));

        if (pRenderersData->m_fDisplayStats == 1) { // Full on-screen statistics
            for (size_t i = 0; i < NB_JITTER; i++) {
                nIndex = (m_upNextSyncOffset + 1 + i) & (NB_JITTER - 1); // modulo action by low bitmask
                Points[i].x  = (float)(StartX + (i * 5));
                Points[i].y  = (float)(StartY + ((m_llSyncOffset[nIndex]) / 2000 + 125));
            }
            m_pLine->Draw(Points, NB_JITTER, D3DCOLOR_XRGB(100, 200, 100));
        }
        m_pLine->End();
    }
}

double CBaseAP::GetRefreshRate()
{
    if (m_pGenlock->powerstripTimingExists) {
        return m_pGenlock->curDisplayFreq;
    } else {
        return m_dD3DRefreshRate;
    }
}

double CBaseAP::GetDisplayCycle()
{
    if (m_pGenlock->powerstripTimingExists) {
        return 1000.0 / m_pGenlock->curDisplayFreq;
    } else {
        return m_dD3DRefreshCycle;
    }
}

double CBaseAP::GetCycleDifference()
{
    double dBaseDisplayCycle = GetDisplayCycle();
    double i;
    double minDiff = 1.0;
    if (!dBaseDisplayCycle || !m_dFrameCycle) {
        return 1.0;
    } else {
        for (i = 1; i <= 8.0; i++) { // Try a lot of multiples of the display frequency
            double dDisplayCycle = i * dBaseDisplayCycle;
            double diff = (dDisplayCycle - m_dFrameCycle) / m_dFrameCycle;
            if (abs(diff) < abs(minDiff)) {
                minDiff = diff;
                m_dOptimumDisplayCycle = dDisplayCycle;
            }
        }
    }
    return minDiff;
}

void CBaseAP::EstimateRefreshTimings()
{
    if (m_pD3DDev) {
        CRenderersData* pRenderersData = GetRenderersData();
        D3DRASTER_STATUS rasterStatus;
        m_pD3DDev->GetRasterStatus(0, &rasterStatus);
        while (rasterStatus.ScanLine) {
            if (m_pD3DDev) { m_pD3DDev->GetRasterStatus(0, &rasterStatus); }
        }
        while (!rasterStatus.ScanLine) {
            if (m_pD3DDev) { m_pD3DDev->GetRasterStatus(0, &rasterStatus); }
        }
        m_pD3DDev->GetRasterStatus(0, &rasterStatus);
        LONGLONG startTime = PerfCounter100ns();
        UINT startLine = rasterStatus.ScanLine;
        LONGLONG endTime = 0;
        LONGLONG time = 0;
        UINT endLine = 0;
        UINT line = 0;
        bool done = false;
        while (!done) { // Estimate time for one scan line
            m_pD3DDev->GetRasterStatus(0, &rasterStatus);
            line = rasterStatus.ScanLine;
            time = PerfCounter100ns();
            if (line > 0) {
                endLine = line;
                endTime = time;
            } else {
                done = true;
            }
        }
        m_dDetectedScanlineTime = (double)(endTime - startTime) / (double)((endLine - startLine) * 10000.0);

        // Estimate the display refresh rate from the vsyncs
        m_pD3DDev->GetRasterStatus(0, &rasterStatus);
        while (rasterStatus.ScanLine) {
            m_pD3DDev->GetRasterStatus(0, &rasterStatus);
        }
        // Now we're at the start of a vsync
        startTime = PerfCounter100ns();
        double i;
        for (i = 1.0; i <= 50.0; i++) {
            m_pD3DDev->GetRasterStatus(0, &rasterStatus);
            while (!rasterStatus.ScanLine) {
                if (m_pD3DDev) { m_pD3DDev->GetRasterStatus(0, &rasterStatus); }
            }
            while (rasterStatus.ScanLine) {
                if (m_pD3DDev) { m_pD3DDev->GetRasterStatus(0, &rasterStatus); }
            }
            // Now we're at the next vsync
        }
        endTime = PerfCounter100ns();
        m_dEstRefreshCycle = static_cast<double>(endTime - startTime) / ((i - 1.0) * 10000.0);
    }
}

#include "../../../thirdparty/VirtualDub/h/vd2/system/cpuaccel.h"

__declspec(nothrow noalias) HRESULT CBaseAP::GetDIB(__out_opt void* pDib, __inout size_t* pSize)
{
    if ((m_u32VideoWidth < 63) || (m_u32VideoHeight < 63)) {
        return E_ABORT;// the copy constructor can't handle this, as I can't be bothered to add the semantics for a small copy target, this also disables the function for an uninitialized renderer state
    }
    size_t upSizeImage = m_u32VideoHeight * m_u32VideoWidth << 2;
    size_t upRequired = sizeof(BITMAPINFOHEADER) + upSizeImage;
    if (!pDib) {
        *pSize = upRequired;
        return S_OK;
    }
    if (*pSize < upRequired) { return E_OUTOFMEMORY; }
    *pSize = upRequired;

    CAutoLock cRenderLock(&m_csAllocatorLock);
    HRESULT hr;
    IDirect3DSurface9* pSurface;
    if (FAILED(hr = m_pD3DDev->CreateOffscreenPlainSurface(m_u32VideoWidth, m_u32VideoHeight, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pSurface, nullptr))) { return hr; }

    if (FAILED(hr = m_pD3DXLoadSurfaceFromSurface(pSurface, nullptr, nullptr, m_apVideoSurface[m_nCurSurface], nullptr, nullptr, (m_dfSurfaceType == D3DFMT_X8R8G8B8) ? D3DX_FILTER_NONE : D3DX_FILTER_POINT | D3DX_FILTER_DITHER, 0))) { goto exit; }

    D3DLOCKED_RECT r;
    if (FAILED(hr = pSurface->LockRect(&r, nullptr, D3DLOCK_READONLY))) { goto exit; }

    BITMAPINFOHEADER* bih = reinterpret_cast<BITMAPINFOHEADER*>(pDib);
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biWidth = m_u32VideoWidth;
    bih->biHeight = m_u32VideoHeight;
    bih->biPlanes = 1;
    bih->biBitCount = 32;
    bih->biCompression = 0;
    bih->biSizeImage = upSizeImage;
    bih->biXPelsPerMeter = 0;
    bih->biYPelsPerMeter = 0;
    bih->biClrUsed = 0;
    bih->biClrImportant = 0;

    // the DIB receiver requires the picture to be upside-down...
    size_t upWidth = m_u32VideoWidth, upHeight = m_u32VideoHeight, upPitch = upWidth << 2, upPitchOffset = r.Pitch - upPitch;
    uintptr_t pSrc = reinterpret_cast<uintptr_t>(r.pBits);// start of the first source line
    uintptr_t pRow = reinterpret_cast<uintptr_t>(bih + 1) + (upHeight - 1) * upPitch; // start of the last line, with padding for the header
    // note: pRow and pDst only keep 4-byte alignment in the realigning loop, for the aligned loop, allocation of the storage makes it 128-byte aligned

    size_t upCount = upWidth;
    uintptr_t pDst = pRow;
    __m128 x0, x1, x2, x3, x4, x5, x6, x7;
    if (CPUGetEnabledExtensions() & CPUF_SUPPORTS_SSE41) {// SSE4.1 code path
        if (upWidth & 15) {// realigning loop
            if (pSrc & 64) {// pre-process if the source needs 64-to-128-byte re-alignment (a 1-in-2 chance on 64-byte cache line systems)
                goto SkipFirst64baSSE41rl;
            }
            goto SkipFirst128baSSE41rl;

            do {
                upCount = upWidth;
                pDst = pRow;
                pSrc += upPitchOffset;// compensate for source pitch

                // fill the output to meet 128-byte alignment, sorted for aligned access
                ASSERT(!(pSrc & 12)); // pitch alignment in GPU memory is always at least 16 bytes, but most will align to 32 bytes
                if (pSrc & 16) {// 16-to-32-byte alignment
                    x1 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc)));
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst), x1);
                    pSrc += 16, pDst += 16;
                    upCount -= 4;
                }
                if (pSrc & 32) {// 32-to-64-byte alignment
                    x2 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc)));
                    x3 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16)));
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst), x2);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 16), x3);
                    pSrc += 32, pDst += 32;
                    upCount -= 8;
                }
                if (pSrc & 64) {// 64-to-128-byte alignment
SkipFirst64baSSE41rl:
                    x4 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc)));
                    x5 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16)));
                    x6 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32)));
                    x7 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48)));
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst), x4);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 16), x5);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 32), x6);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 48), x7);
                    pSrc += 64, pDst += 64;
                    upCount -= 16;
                }
SkipFirst128baSSE41rl:
                ASSERT(!(pSrc & 127)); // if not 128-byte aligned, the loop is implemented wrong

                size_t j = upCount >> 5;
                do {
                    x0 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc)));
                    x1 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16)));
                    x2 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32)));
                    x3 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48)));
                    x4 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 64)));
                    x5 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 80)));
                    x6 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 96)));
                    x7 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 112)));
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst), x0);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 16), x1);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 32), x2);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 48), x3);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 64), x4);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 80), x5);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 96), x6);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 112), x7);
                    pSrc += 128, pDst += 128;
                } while (--j);

                // finalize the last 31 optional values, sorted for aligned access
                if (upCount & 16) {
                    x0 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc)));
                    x1 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16)));
                    x2 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32)));
                    x3 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48)));
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst), x0);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 16), x1);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 32), x2);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 48), x3);
                    pSrc += 64, pDst += 64;
                }
                if (upCount & 15) {
                    x4 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc)));
                    x5 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16)));
                    x6 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32)));
                    x7 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48)));
                    if (upCount & 8) {
                        _mm_storeu_ps(reinterpret_cast<float*>(pDst), x4);
                        _mm_storeu_ps(reinterpret_cast<float*>(pDst + 16), x5);
                        pSrc += 32, pDst += 32;
                    }
                    if (upCount & 4) {
                        _mm_storeu_ps(reinterpret_cast<float*>(pDst), x6);
                        pSrc += 16, pDst += 16;
                    }
                    if (upCount & 2) {
                        _mm_storel_pi(reinterpret_cast<__m64*>(pDst), x7);// not an MMX function
                        x7 = _mm_movehl_ps(x7, x7);// move high part to low
                        pSrc += 8, pDst += 8;
                    }
                    if (upCount & 1) {
                        _mm_store_ss(reinterpret_cast<float*>(pDst), x7);
                        pSrc += 4;// no pDst address increment for the last possible value
                    }
                }

                pRow -= upPitch;
            } while (--upHeight);
        } else {// aligned loop
            ASSERT(!upPitchOffset);// high alignment and locking an entire texture means pitch == width*unitsize
            upHeight -= 62;
            if (pSrc & 64) {// pre-load if the source needs 64-to-128-byte re-alignment (a 1-in-2 chance on 64-byte cache line systems)
                x4 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc)));
                x5 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16)));
                x6 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32)));
                x7 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48)));
                goto SkipFirst64baSSE41al;
            }
            goto SkipFirst128baSSE41al;

            do {// streaming the larger bottom part
                upCount = upWidth;
                pDst = pRow;

                if (pSrc & 64) {// 64-to-128-byte alignment
SkipFirst64baSSE41al:
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 64), x4);
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 80), x5);
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 96), x6);
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 112), x7);
                    pSrc += 64, pDst += 64;
                    upCount -= 16;
                }
SkipFirst128baSSE41al:
                ASSERT(!(pSrc & 127)); // if not 128-byte aligned, the loop is implemented wrong

                size_t j = upCount >> 5;
                do {
                    x0 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc)));
                    x1 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16)));
                    x2 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32)));
                    x3 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48)));
                    x4 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 64)));
                    x5 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 80)));
                    x6 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 96)));
                    x7 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 112)));
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

                if (upCount & 16) {// finalize the last 16 optional values
                    x0 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc)));
                    x1 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16)));
                    x2 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32)));
                    x3 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48)));
                    x4 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 64)));
                    x5 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 80)));
                    x6 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 96)));
                    x7 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 112)));
                    pSrc += 64;
                    _mm_stream_ps(reinterpret_cast<float*>(pDst), x0);
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 16), x1);
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 32), x2);
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 48), x3);
                }// no pDst address increment for the last possible value

                pRow -= upPitch;
            } while (--upHeight);

            upHeight = 62;
            do {// storing the top part, this will usually be the first set of data to be re-requested
                upCount = upWidth;
                pDst = pRow;

                if (pSrc & 64) {// 64-to-128-byte alignment
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 64), x4);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 80), x5);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 96), x6);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 112), x7);
                    pSrc += 64, pDst += 64;
                    upCount -= 16;
                }

                size_t j = upCount >> 5;
                do {
                    x0 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc)));
                    x1 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16)));
                    x2 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32)));
                    x3 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48)));
                    x4 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 64)));
                    x5 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 80)));
                    x6 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 96)));
                    x7 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 112)));
                    _mm_store_ps(reinterpret_cast<float*>(pDst), x0);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 16), x1);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 32), x2);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 48), x3);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 64), x4);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 80), x5);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 96), x6);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 112), x7);
                    pSrc += 128, pDst += 128;
                } while (--j);

                if (upCount & 16) {// finalize the last 16 optional values
                    x0 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc)));
                    x1 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16)));
                    x2 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32)));
                    x3 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48)));
                    x4 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 64)));
                    x5 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 80)));
                    x6 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 96)));
                    x7 = _mm_castsi128_ps(_mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 112)));
                    pSrc += 64;
                    _mm_store_ps(reinterpret_cast<float*>(pDst), x0);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 16), x1);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 32), x2);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 48), x3);
                }// no pDst address increment for the last possible value

                pRow -= upPitch;
            } while (--upHeight);
        }
    } else {// SSE code path
        if (upWidth & 15) {// realigning loop
            if (pSrc & 64) {// pre-process if the source needs 64-to-128-byte re-alignment (a 1-in-2 chance on 64-byte cache line systems)
                goto SkipFirst64baSSErl;
            }
            goto SkipFirst128baSSErl;

            do {
                upCount = upWidth;
                pDst = pRow;
                pSrc += upPitchOffset;// compensate for source pitch

                // fill the output to meet 128-byte alignment, sorted for aligned access
                ASSERT(!(pSrc & 12)); // pitch alignment in GPU memory is always at least 16 bytes, but most will align to 32 bytes
                if (pSrc & 16) {// 16-to-32-byte alignment
                    x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst), x1);
                    pSrc += 16, pDst += 16;
                    upCount -= 4;
                }
                if (pSrc & 32) {// 32-to-64-byte alignment
                    x2 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                    x3 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst), x2);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 16), x3);
                    pSrc += 32, pDst += 32;
                    upCount -= 8;
                }
                if (pSrc & 64) {// 64-to-128-byte alignment
SkipFirst64baSSErl:
                    x4 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                    x5 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                    x6 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 32));
                    x7 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 48));
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst), x4);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 16), x5);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 32), x6);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 48), x7);
                    pSrc += 64, pDst += 64;
                    upCount -= 16;
                }
SkipFirst128baSSErl:
                ASSERT(!(pSrc & 127)); // if not 128-byte aligned, the loop is implemented wrong

                size_t j = upCount >> 5;
                do {
                    x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                    x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                    x2 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 32));
                    x3 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 48));
                    x4 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 64));
                    x5 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 80));
                    x6 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 96));
                    x7 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 112));
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst), x0);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 16), x1);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 32), x2);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 48), x3);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 64), x4);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 80), x5);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 96), x6);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 112), x7);
                    pSrc += 128, pDst += 128;
                } while (--j);

                // finalize the last 31 optional values, sorted for aligned access
                if (upCount & 16) {
                    x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                    x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                    x2 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 32));
                    x3 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 48));
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst), x0);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 16), x1);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 32), x2);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 48), x3);
                    pSrc += 64, pDst += 64;
                }
                if (upCount & 15) {
                    x4 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                    x5 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                    x6 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 32));
                    x7 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 48));
                    if (upCount & 8) {
                        _mm_storeu_ps(reinterpret_cast<float*>(pDst), x4);
                        _mm_storeu_ps(reinterpret_cast<float*>(pDst + 16), x5);
                        pSrc += 32, pDst += 32;
                    }
                    if (upCount & 4) {
                        _mm_storeu_ps(reinterpret_cast<float*>(pDst), x6);
                        pSrc += 16, pDst += 16;
                    }
                    if (upCount & 2) {
                        _mm_storel_pi(reinterpret_cast<__m64*>(pDst), x7);// not an MMX function
                        x7 = _mm_movehl_ps(x7, x7);// move high part to low
                        pSrc += 8, pDst += 8;
                    }
                    if (upCount & 1) {
                        _mm_store_ss(reinterpret_cast<float*>(pDst), x7);
                        pSrc += 4;// no pDst address increment for the last possible value
                    }
                }

                pRow -= upPitch;
            } while (--upHeight);
        } else {// aligned loop
            ASSERT(!upPitchOffset);// high alignment and locking an entire texture means pitch == width*unitsize
            upHeight -= 62;
            if (pSrc & 64) {// pre-load if the source needs 64-to-128-byte re-alignment (a 1-in-2 chance on 64-byte cache line systems)
                x4 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                x5 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                x6 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 32));
                x7 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 48));
                goto SkipFirst64baSSEal;
            }
            goto SkipFirst128baSSEal;

            do {// streaming the larger bottom part
                upCount = upWidth;
                pDst = pRow;

                if (pSrc & 64) {// 64-to-128-byte alignment
SkipFirst64baSSEal:
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 64), x4);
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 80), x5);
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 96), x6);
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 112), x7);
                    pSrc += 64, pDst += 64;
                    upCount -= 16;
                }
SkipFirst128baSSEal:
                ASSERT(!(pSrc & 127)); // if not 128-byte aligned, the loop is implemented wrong

                size_t j = upCount >> 5;
                do {
                    x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                    x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                    x2 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 32));
                    x3 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 48));
                    x4 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 64));
                    x5 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 80));
                    x6 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 96));
                    x7 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 112));
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

                if (upCount & 16) {// finalize the last 16 optional values
                    x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                    x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                    x2 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 32));
                    x3 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 48));
                    x4 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 64));
                    x5 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 80));
                    x6 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 96));
                    x7 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 112));
                    pSrc += 64;
                    _mm_stream_ps(reinterpret_cast<float*>(pDst), x0);
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 16), x1);
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 32), x2);
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 48), x3);
                }// no pDst address increment for the last possible value

                pRow -= upPitch;
            } while (--upHeight);

            upHeight = 62;
            do {// storing the top part, this will usually be the first set of data to be re-requested
                upCount = upWidth;
                pDst = pRow;

                if (pSrc & 64) {// 64-to-128-byte alignment
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 64), x4);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 80), x5);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 96), x6);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 112), x7);
                    pSrc += 64, pDst += 64;
                    upCount -= 16;
                }

                size_t j = upCount >> 5;
                do {
                    x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                    x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                    x2 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 32));
                    x3 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 48));
                    x4 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 64));
                    x5 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 80));
                    x6 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 96));
                    x7 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 112));
                    _mm_store_ps(reinterpret_cast<float*>(pDst), x0);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 16), x1);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 32), x2);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 48), x3);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 64), x4);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 80), x5);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 96), x6);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 112), x7);
                    pSrc += 128, pDst += 128;
                } while (--j);

                if (upCount & 16) {// finalize the last 16 optional values
                    x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                    x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                    x2 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 32));
                    x3 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 48));
                    x4 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 64));
                    x5 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 80));
                    x6 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 96));
                    x7 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 112));
                    pSrc += 64;
                    _mm_store_ps(reinterpret_cast<float*>(pDst), x0);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 16), x1);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 32), x2);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 48), x3);
                }// no pDst address increment for the last possible value

                pRow -= upPitch;
            } while (--upHeight);
        }
    }

    hr = pSurface->UnlockRect();
exit:
    pSurface->Release();
    return hr;
}

__declspec(nothrow noalias) HRESULT CBaseAP::SetPixelShader(__in CStringW const* pstrSrcData, __in unsigned __int8 u8RenderStage)
{
    CAutoLock cRenderLock(&m_csAllocatorLock);

    if (!pstrSrcData) {// no string, clear the pixel shader stage
        POSITION pos = m_apCustomPixelShaders[u8RenderStage].GetHeadPosition();
        while (pos) {
            EXTERNALSHADER& ceShader = m_apCustomPixelShaders[u8RenderStage].GetNext(pos);
            if (ceShader.pPixelShader) {
                ceShader.pPixelShader->Release();
            }
            delete[] ceShader.strSrcData;
        }
        m_apCustomPixelShaders[u8RenderStage].RemoveAll();
        return S_OK;
    }

    // allocate
    UINT i = pstrSrcData->GetLength();
    char* pDst = DEBUG_NEW char[i];// note: we don't have to append a trailing nullptr
    if (!pDst) {
        return E_OUTOFMEMORY;
    }
    POSITION pos = m_apCustomPixelShaders[u8RenderStage].AddTail();// add an empty item
    if (!pos) {
        delete[] pDst;
        return E_OUTOFMEMORY;
    }

    // fill
    EXTERNALSHADER& ceShader = m_apCustomPixelShaders[u8RenderStage].GetAt(pos);
    ceShader.pPixelShader = nullptr;// doesn't have a default initializer
    ceShader.strSrcData = pDst;
    ceShader.uiSrcLen = i;
    // copy while quickly converting wchar_t to char
    char const* pSrc = reinterpret_cast<char const*>(static_cast<wchar_t const*>(*pstrSrcData));
    do {
        *pDst++ = pSrc[0] | pSrc[1];// higher Unicode pages may have valid characters with the bottom 8 bits 0, to not possibly embed a nullptr here, OR the lower and higher bits
        pSrc += 2;
    } while (--i);
    return S_OK;
}

CSyncAP::CSyncAP(HWND hWnd, HRESULT* phr, CString& _Error)
    : CBaseAP(hWnd, phr, _Error)
    // insert the nullptr-initialized COM pointers here, in exactly the same order as present in the class
    , m_pMixerType(nullptr)
    , m_pMixer(nullptr)
    , m_pSink(nullptr)
    , m_pClock(nullptr)
    , m_pD3DManager(nullptr)
{
    HINSTANCE hLib;

    m_nResetToken = 0;
    m_hRenderThread  = INVALID_HANDLE_VALUE;
    m_hMixerThread = INVALID_HANDLE_VALUE;
    m_hEvtFlush = INVALID_HANDLE_VALUE;
    m_hEvtQuit = INVALID_HANDLE_VALUE;
    m_hEvtSkip = INVALID_HANDLE_VALUE;
    m_bEvtQuit = 0;
    m_bEvtFlush = 0;

    if (FAILED(*phr)) {
        _Error += L"SyncAP failed\n";
        return;
    }

    // Load EVR specifics DLLs
    hLib = LoadLibrary(L"dxva2.dll");
    pfDXVA2CreateDirect3DDeviceManager9 = hLib ? (PTR_DXVA2CreateDirect3DDeviceManager9) GetProcAddress(hLib, "DXVA2CreateDirect3DDeviceManager9") : nullptr;

    // Load EVR functions
    hLib = LoadLibrary(L"evr.dll");
    pfMFCreateDXSurfaceBuffer = hLib ? (PTR_MFCreateDXSurfaceBuffer)GetProcAddress(hLib, "MFCreateDXSurfaceBuffer") : nullptr;
    pfMFCreateVideoSampleFromSurface = hLib ? (PTR_MFCreateVideoSampleFromSurface)GetProcAddress(hLib, "MFCreateVideoSampleFromSurface") : nullptr;

    if (!pfDXVA2CreateDirect3DDeviceManager9 || !pfMFCreateDXSurfaceBuffer || !pfMFCreateVideoSampleFromSurface) {
        if (!pfDXVA2CreateDirect3DDeviceManager9) {
            _Error += L"Could not find DXVA2CreateDirect3DDeviceManager9 (dxva2.dll)\n";
        }
        if (!pfMFCreateDXSurfaceBuffer) {
            _Error += L"Could not find MFCreateDXSurfaceBuffer (evr.dll)\n";
        }
        if (!pfMFCreateVideoSampleFromSurface) {
            _Error += L"Could not find MFCreateVideoSampleFromSurface (evr.dll)\n";
        }
        *phr = E_FAIL;
        return;
    }

    // Load Vista specific DLLs
    hLib = LoadLibrary(L"avrt.dll");
    pfAvSetMmThreadCharacteristicsW = hLib ? (PTR_AvSetMmThreadCharacteristicsW) GetProcAddress(hLib, "AvSetMmThreadCharacteristicsW") : nullptr;
    pfAvSetMmThreadPriority = hLib ? (PTR_AvSetMmThreadPriority) GetProcAddress(hLib, "AvSetMmThreadPriority") : nullptr;
    pfAvRevertMmThreadCharacteristics = hLib ? (PTR_AvRevertMmThreadCharacteristics) GetProcAddress(hLib, "AvRevertMmThreadCharacteristics") : nullptr;

    // Init DXVA manager
    *phr = pfDXVA2CreateDirect3DDeviceManager9(&m_nResetToken, &m_pD3DManager);
    if (SUCCEEDED(*phr)) {
        *phr = m_pD3DManager->ResetDevice(m_pD3DDev, m_nResetToken);
        if (!SUCCEEDED(*phr)) {
            _Error += L"m_pD3DManager->ResetDevice failed\n";
        }
    } else {
        _Error += L"DXVA2CreateDirect3DDeviceManager9 failed\n";
    }

    CComPtr<IDirectXVideoDecoderService> pDecoderService;
    HANDLE hDevice;
    if (SUCCEEDED(m_pD3DManager->OpenDeviceHandle(&hDevice)) &&
            SUCCEEDED(m_pD3DManager->GetVideoService(hDevice, IID_IDirectXVideoDecoderService, (void**)&pDecoderService))) {
        HookDirectXVideoDecoderService(pDecoderService);
        m_pD3DManager->CloseDeviceHandle(hDevice);
    }

    m_nDXSurface = mk_pRendererSettings->MixerBuffers;

    m_nRenderState = Shutdown;
    m_bStepping = false;
    m_bUseInternalTimer = false;
    m_bPendingRenegotiate = false;
    m_bPendingMediaFinished = false;
    m_nStepCount = 0;
    m_dwVideoRenderPrefs = (MFVideoRenderPrefs)0;
    m_BorderColor = RGB(0, 0, 0);
    m_pOuterEVR = nullptr;
    m_bPrerolled = false;
    m_lShiftToNearest = -1; // Illegal value to start with
}

CSyncAP::~CSyncAP()
{
    StopWorkerThreads();
    if (m_pMixerType) { m_pMixerType->Release(); }// no assertion on the reference count
    ASSERT(!m_pMixer);// the external EVR will always call ReleaseServicePointers() when closing
    ASSERT(!m_pSink);
    ASSERT(!m_pClock);
    ULONG u;
    if (m_pD3DManager) {
        u = m_pD3DManager->Release();
        ASSERT(!u);
    }
    m_pRefClock = nullptr;// from external

    UnhookNewSegmentAndReceive();
}

HRESULT CSyncAP::CheckShutdown() const
{
    if (m_nRenderState == Shutdown) {
        return MF_E_SHUTDOWN;
    } else {
        return S_OK;
    }
}

void CSyncAP::StartWorkerThreads()
{
    if (m_nRenderState == Shutdown) {
        m_hEvtQuit = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        m_hEvtFlush = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        m_hEvtSkip = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        m_hMixerThread = ::CreateThread(nullptr, 0x10000, MixerThreadStatic, this, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
        SetThreadPriority(m_hMixerThread, THREAD_PRIORITY_HIGHEST);
        m_hRenderThread = ::CreateThread(nullptr, 0x20000, RenderThreadStatic, this, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
        SetThreadPriority(m_hRenderThread, THREAD_PRIORITY_TIME_CRITICAL);
        m_nRenderState = Stopped;
    }
}

void CSyncAP::StopWorkerThreads()
{
    if (m_nRenderState != Shutdown) {
        SetEvent(m_hEvtFlush);
        m_bEvtFlush = true;
        SetEvent(m_hEvtQuit);
        m_bEvtQuit = true;
        SetEvent(m_hEvtSkip);
        m_bEvtSkip = true;
        if ((m_hRenderThread != INVALID_HANDLE_VALUE) && (WaitForSingleObject(m_hRenderThread, 10000) == WAIT_TIMEOUT)) {
            ASSERT(FALSE);
            TerminateThread(m_hRenderThread, 0xDEAD);
        }
        if (m_hRenderThread != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hRenderThread);
        }
        if ((m_hMixerThread != INVALID_HANDLE_VALUE) && (WaitForSingleObject(m_hMixerThread, 10000) == WAIT_TIMEOUT)) {
            ASSERT(FALSE);
            TerminateThread(m_hMixerThread, 0xDEAD);
        }
        if (m_hMixerThread != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hMixerThread);
        }

        if (m_hEvtFlush != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hEvtFlush);
        }
        if (m_hEvtQuit != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hEvtQuit);
        }
        if (m_hEvtSkip != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hEvtSkip);
        }

        m_bEvtFlush = false;
        m_bEvtQuit = false;
        m_bEvtSkip = false;
    }
    m_nRenderState = Shutdown;
}

__declspec(nothrow noalias) HRESULT CSyncAP::CreateRenderer(__out_opt IBaseFilter** ppRenderer)
{
    if (!ppRenderer) {
        return E_POINTER;
    }
    *ppRenderer = nullptr;
    HRESULT hr = E_FAIL;

    do {
        CMacrovisionKicker* pMK = DEBUG_NEW CMacrovisionKicker();

        __assume(this);// fix assembly: the compiler generated tests for null pointer input on static_cast<T>(this)
        CSyncRenderer* pOuterEVR = DEBUG_NEW CSyncRenderer(NAME("CSyncRenderer"), static_cast<IUnknown*>(pMK), hr, &m_abVMR9AlphaBitmap, this);
        m_pOuterEVR = pOuterEVR;

        pMK->SetInner((IUnknown*)(INonDelegatingUnknown*)pOuterEVR);
        CComQIPtr<IBaseFilter> pBF = pMK;
        pMK->Release();

        if (FAILED(hr)) {
            break;
        }

        // Set EVR custom presenter
        CComPtr<IMFVideoPresenter> pVP;
        CComPtr<IMFVideoRenderer> pMFVR;
        CComQIPtr<IMFGetService> pMFGS = pBF;
        CComQIPtr<IEVRFilterConfig> pConfig = pBF;
        if (SUCCEEDED(hr)) {
            if (FAILED(pConfig->SetNumberOfStreams(3))) { // TODO - maybe need other number of input stream ...
                break;
            }
        }

        hr = pMFGS->GetService(MR_VIDEO_RENDER_SERVICE, IID_IMFVideoRenderer, (void**)&pMFVR);

        if (SUCCEEDED(hr)) {
            hr = QueryInterface(IID_IMFVideoPresenter, (void**)&pVP);
        }
        if (SUCCEEDED(hr)) {
            hr = pMFVR->InitializeRenderer(nullptr, pVP);
        }

        CComPtr<IPin> pPin = GetFirstPin(pBF);
        CComQIPtr<IMemInputPin> pMemInputPin = pPin;

        m_bUseInternalTimer = HookNewSegmentAndReceive(pPin, pMemInputPin);
        if (FAILED(hr)) {
            *ppRenderer = nullptr;
        } else {
            *ppRenderer = pBF.Detach();
        }
    } while (0);

    return hr;
}

// IUnknown

__declspec(nothrow noalias) STDMETHODIMP CSyncAP::QueryInterface(REFIID riid, __deref_out void** ppv)
{
    ASSERT(ppv);

    __assume(this);// fix assembly: the compiler generated tests for null pointer input on static_cast<T>(this)
    if (riid == IID_IDirect3DDeviceManager9) {
        if (m_pD3DManager) { return m_pD3DManager->QueryInterface(IID_IDirect3DDeviceManager9, ppv); }
        return E_UNEXPECTED;
    }

    if (riid == IID_IUnknown) { *ppv = static_cast<IUnknown*>(static_cast<CSubPicAllocatorPresenterImpl*>(this)); }// CSubPicAllocatorPresenterImpl is at Vtable location 0
    else if (riid == __uuidof(CSubPicAllocatorPresenterImpl)) { *ppv = static_cast<CSubPicAllocatorPresenterImpl*>(this); }
    else if (riid == IID_IMFClockStateSink) { *ppv = static_cast<IMFClockStateSink*>(this); }
    else if (riid == IID_IMFVideoPresenter) { *ppv = static_cast<IMFVideoPresenter*>(this); }
    else if (riid == IID_IMFTopologyServiceLookupClient) { *ppv = static_cast<IMFTopologyServiceLookupClient*>(this); }
    else if (riid == IID_IMFVideoDeviceID) { *ppv = static_cast<IMFVideoDeviceID*>(this); }
    else if (riid == IID_IMFGetService) { *ppv = static_cast<IMFGetService*>(this); }
    else if (riid == IID_IMFAsyncCallback) { *ppv = static_cast<IMFAsyncCallback*>(this); }
    else if (riid == IID_IMFVideoDisplayControl) { *ppv = static_cast<IMFVideoDisplayControl*>(this); }
    else if (riid == IID_IEVRTrustedVideoPlugin) { *ppv = static_cast<IEVRTrustedVideoPlugin*>(this); }
    else if (riid == IID_IQualProp) { *ppv = static_cast<IQualProp*>(this); }
    else if (riid == IID_IMFRateSupport) { *ppv = static_cast<IMFRateSupport*>(this); }
    else if (riid == __uuidof(ISyncClockAdviser)) { *ppv = static_cast<ISyncClockAdviser*>(this); }
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    UNREFERENCED_PARAMETER(ulRef);
    return NOERROR;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CSyncAP::AddRef()
{
    // based on CUnknown::NonDelegatingAddRef()
    // the original CUnknown::NonDelegatingAddRef() has a version that keeps compatibility for Windows 95, Windows NT 3.51 and earlier, this one doesn't
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    return ulRef;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CSyncAP::Release()
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
        this->~CSyncAP();
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
STDMETHODIMP CSyncAP::OnClockStart(MFTIME hnsSystemTime,  LONGLONG llClockStartOffset)
{
    m_nRenderState = Started;
    return S_OK;
}

STDMETHODIMP CSyncAP::OnClockStop(MFTIME hnsSystemTime)
{
    m_nRenderState = Stopped;
    return S_OK;
}

STDMETHODIMP CSyncAP::OnClockPause(MFTIME hnsSystemTime)
{
    m_nRenderState = Paused;
    return S_OK;
}

STDMETHODIMP CSyncAP::OnClockRestart(MFTIME hnsSystemTime)
{
    m_nRenderState  = Started;
    return S_OK;
}

STDMETHODIMP CSyncAP::OnClockSetRate(MFTIME hnsSystemTime, float flRate)
{
    return E_NOTIMPL;
}

// IBaseFilter delegate
bool CSyncAP::GetState(DWORD dwMilliSecsTimeout, FILTER_STATE* State, HRESULT& _ReturnValue)
{
    CAutoLock lock(&m_SampleQueueLock);
    switch (m_nRenderState) {
        case Started:
            *State = State_Running;
            break;
        case Paused:
            *State = State_Paused;
            break;
        case Stopped:
            *State = State_Stopped;
            break;
        default:
            *State = State_Stopped;
            _ReturnValue = E_FAIL;
    }
    _ReturnValue = S_OK;
    return true;
}

// IQualProp
STDMETHODIMP CSyncAP::get_FramesDroppedInRenderer(int* pcFrames)
{
    *pcFrames = m_pcFramesDropped;
    return S_OK;
}

STDMETHODIMP CSyncAP::get_FramesDrawn(int* pcFramesDrawn)
{
    *pcFramesDrawn = m_pcFramesDrawn;
    return S_OK;
}

STDMETHODIMP CSyncAP::get_AvgFrameRate(int* piAvgFrameRate)
{
    *piAvgFrameRate = (int)(m_dAverageFrameRate * 100);
    return S_OK;
}

STDMETHODIMP CSyncAP::get_Jitter(int* iJitter)
{
    *iJitter = (int)((m_dJitterStdDev / 10000.0) + 0.5);
    return S_OK;
}

STDMETHODIMP CSyncAP::get_AvgSyncOffset(int* piAvg)
{
    *piAvg = (int)((m_dSyncOffsetAvr / 10000.0) + 0.5);
    return S_OK;
}

STDMETHODIMP CSyncAP::get_DevSyncOffset(int* piDev)
{
    *piDev = (int)((m_dSyncOffsetStdDev / 10000.0) + 0.5);
    return S_OK;
}

// IMFRateSupport
STDMETHODIMP CSyncAP::GetSlowestRate(MFRATE_DIRECTION eDirection, BOOL fThin, float* pflRate)
{
    *pflRate = 0;
    return S_OK;
}

STDMETHODIMP CSyncAP::GetFastestRate(MFRATE_DIRECTION eDirection, BOOL fThin, float* pflRate)
{
    if (!pflRate) { return E_POINTER; }
    HRESULT hr;
    if (FAILED(hr = CheckShutdown())) { return hr; }

    // Get the maximum forward rate.
    *pflRate = GetMaxRate(fThin);
    // For reverse playback, swap the sign.
    if (eDirection == MFRATE_REVERSE) { *pflRate = -*pflRate; }
    return S_OK;
}

STDMETHODIMP CSyncAP::IsRateSupported(BOOL fThin, float flRate, float* pflNearestSupportedRate)
{
    if (!pflNearestSupportedRate) { return E_POINTER; }
    HRESULT hr;
    if (FAILED(hr = CheckShutdown())) { return hr; }
    // fRate can be negative for reverse playback.
    // pfNearestSupportedRate can be nullptr.

    float   fMaxRate = 0.0f;
    float   fNearestRate = flRate;   // Default.

    // Find the maximum forward rate.
    fMaxRate = GetMaxRate(fThin);

    if (fabsf(flRate) > fMaxRate) {
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
    if (pflNearestSupportedRate != nullptr) {
        *pflNearestSupportedRate = fNearestRate;
    }
    return hr;
}

float CSyncAP::GetMaxRate(BOOL bThin)
{
    float fMaxRate = FLT_MAX;  // Default.
    UINT32 fpsNumerator = 0, fpsDenominator = 0;

    if (!bThin && m_pMixerType) {
        // Non-thinned: Use the frame rate and monitor refresh rate.

        // Frame rate:
        MFGetAttributeRatio(m_pMixerType, MF_MT_FRAME_RATE,
                            &fpsNumerator, &fpsDenominator);

        if (fpsDenominator && fpsNumerator && m_dD3DRefreshRate) {
            // Max Rate = Refresh Rate / Frame Rate
            fMaxRate = MulDiv(m_dD3DRefreshRate, fpsDenominator, fpsNumerator);
        }
    }
    return fMaxRate;
}

void CSyncAP::CompleteFrameStep(bool bCancel)
{
    if (m_nStepCount > 0) {
        if (bCancel || (m_nStepCount == 1)) {
            m_pSink->Notify(EC_STEP_COMPLETE, bCancel ? TRUE : FALSE, 0);
            m_nStepCount = 0;
        } else {
            --m_nStepCount;
        }
    }
}

// IMFVideoPresenter
STDMETHODIMP CSyncAP::ProcessMessage(MFVP_MESSAGE_TYPE eMessage, ULONG_PTR ulParam)
{
    HRESULT hr = S_OK;

    switch (eMessage) {
        case MFVP_MESSAGE_BEGINSTREAMING:
            hr = BeginStreaming();
            m_llHysteresis = 0;
            m_lShiftToNearest = 0;
            m_bStepping = false;
            break;

        case MFVP_MESSAGE_CANCELSTEP:
            m_bStepping = false;
            CompleteFrameStep(true);
            break;

        case MFVP_MESSAGE_ENDOFSTREAM:
            m_bPendingMediaFinished = true;
            break;

        case MFVP_MESSAGE_ENDSTREAMING:
            m_pGenlock->ResetTiming();
            m_pRefClock = nullptr;
            break;

        case MFVP_MESSAGE_FLUSH:
            SetEvent(m_hEvtFlush);
            m_bEvtFlush = true;
            while (WaitForSingleObject(m_hEvtFlush, 1) == WAIT_OBJECT_0) {
                ;
            }
            break;

        case MFVP_MESSAGE_INVALIDATEMEDIATYPE:
            m_bPendingRenegotiate = true;
            while (*((volatile bool*)&m_bPendingRenegotiate))   ;
            break;

        case MFVP_MESSAGE_PROCESSINPUTNOTIFY:
            break;

        case MFVP_MESSAGE_STEP:
            m_nStepCount = ulParam;
            m_bStepping = true;
            break;

        default:
            ASSERT(FALSE);
            break;
    }
    return hr;
}

static MFOffset MakeOffset(double v)
{
    MFOffset offset;
    offset.value = short(v);
    offset.fract = WORD(65536 * (v - offset.value));
    return offset;
}

static MFVideoArea MakeArea(double x, double y, LONG width, LONG height)
{
    MFVideoArea area;
    area.OffsetX = MakeOffset(x);
    area.OffsetY = MakeOffset(y);
    area.Area.cx = width;
    area.Area.cy = height;
    return area;
}

__declspec(nothrow noalias) __int8 CSyncAP::GetMediaTypeMerit(IMFMediaType* pMixerType)
{
    GUID guidSubType;
    if (SUCCEEDED(pMixerType->GetGUID(MF_MT_SUBTYPE, &guidSubType))) {
        DWORD dwSubType = guidSubType.Data1;
        return (dwSubType == MFVideoFormat_AI44.Data1) ? 32 // Palettized, 4:4:4
               : (dwSubType == MFVideoFormat_YVU9.Data1) ? 31 // 8-bit, 16:1:1
               : (dwSubType == MFVideoFormat_NV11.Data1) ? 30 // 8-bit, 4:1:1
               : (dwSubType == MFVideoFormat_Y41P.Data1) ? 29
               : (dwSubType == MFVideoFormat_Y41T.Data1) ? 28
               : (dwSubType == MFVideoFormat_P016.Data1) ? 27 // 4:2:0
               : (dwSubType == MFVideoFormat_P010.Data1) ? 26
               : (dwSubType == MFVideoFormat_NV12.Data1) ? 25
               : (dwSubType == MFVideoFormat_I420.Data1) ? 24
               : (dwSubType == MFVideoFormat_IYUV.Data1) ? 23
               : (dwSubType == MFVideoFormat_YV12.Data1) ? 22
               : (dwSubType == MFVideoFormat_Y216.Data1) ? 21 // 4:2:2
               : (dwSubType == MFVideoFormat_P216.Data1) ? 20
               : (dwSubType == MFVideoFormat_v216.Data1) ? 19
               : (dwSubType == MFVideoFormat_Y210.Data1) ? 18
               : (dwSubType == MFVideoFormat_P210.Data1) ? 17
               : (dwSubType == MFVideoFormat_v210.Data1) ? 16
               : (dwSubType == MFVideoFormat_YUY2.Data1) ? 15
               : (dwSubType == MFVideoFormat_YVYU.Data1) ? 14
               : (dwSubType == MFVideoFormat_UYVY.Data1) ? 13
               : (dwSubType == MFVideoFormat_Y42T.Data1) ? 12
               : (dwSubType == MFVideoFormat_Y416.Data1) ? 11 // 4:4:4
               : (dwSubType == MFVideoFormat_Y410.Data1) ? 10
               : (dwSubType == MFVideoFormat_v410.Data1) ? 9
               : (dwSubType == MFVideoFormat_AYUV.Data1) ? 0 // To keep track of for further developments: there's currently no GPU available with native support of AYUV surfaces, to prevent the software mixer from using a very slow converter to X8R8G8B8, AYUV is disabled for now
               : (dwSubType == MFVideoFormat_RGB32.Data1) ? 6 // always rank RGB types lower than the rest of the types to avoid the many problems with RGB conversions before the mixer
               : (dwSubType == MFVideoFormat_RGB24.Data1) ? 5
               : (dwSubType == MFVideoFormat_ARGB32.Data1) ? 4
               : (dwSubType == MFVideoFormat_RGB565.Data1) ? 3
               : (dwSubType == MFVideoFormat_RGB555.Data1) ? 2
               : (dwSubType == MFVideoFormat_RGB8.Data1) ? 1
               : 7;
    }
    return 0;
}

HRESULT CSyncAP::RenegotiateMediaType()
{
    if (!m_pMixer) {
        ASSERT(0);
        return MF_E_INVALIDREQUEST;
    }

    // Loop through all of the mixer's proposed output types.
    HRESULT hr;
    GUID guidSubType;
    CInterfaceArray<IMFMediaType> paValidMixerTypes;
    DWORD dwTypeIndex = 0;
    while (1) {
        // Step 1. Get the next media type supported by mixer
        if (m_pMixerType) {
            m_pMixerType->Release();// no assertion on the reference count
            m_pMixerType = nullptr;
        }
        hr = m_pMixer->GetOutputAvailableType(0, dwTypeIndex, &m_pMixerType);// will output hr = MF_E_NO_MORE_TYPES when no more types are available
        if (hr == MF_E_NO_MORE_TYPES) { break; }// m_pMixerType will always be nullptr on breaking
        ++dwTypeIndex;
        if (FAILED(hr)) { continue; }

        // Step 2. Construct a working media type
        UINT32 uiFw, uiFh, uiARx, uiARy;
        if (FAILED(hr = m_pMixerType->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_0_255))) { continue; }
        if (FAILED(hr = MFGetAttributeSize(m_pMixerType, MF_MT_FRAME_SIZE, &uiFw, &uiFh))) { continue; }
        const LARGE_INTEGER FrameSize = {uiFh, uiFw};
        if (FAILED(hr = m_pMixerType->SetUINT64(MF_MT_FRAME_SIZE, FrameSize.QuadPart))) { continue; }
        const MFVideoArea pArea = MakeArea(0.0, 0.0, uiFw, uiFh);
        if (FAILED(hr = m_pMixerType->SetBlob(MF_MT_GEOMETRIC_APERTURE, reinterpret_cast<const UINT8*>(&pArea), sizeof(MFVideoArea)))) { continue; }
        if (FAILED(hr = MFGetAttributeSize(m_pMixerType, MF_MT_PIXEL_ASPECT_RATIO, &uiARx, &uiARy))) { continue; }
        const LARGE_INTEGER AspectRatio = {uiARy, uiARx};
        if (FAILED(hr = m_pMixerType->SetUINT64(MF_MT_PIXEL_ASPECT_RATIO, AspectRatio.QuadPart))) { continue; }

        // Step 3. Check if the mixer will accept this media type
        if (FAILED(hr = m_pMixer->SetOutputType(0, m_pMixerType, MFT_SET_TYPE_TEST_ONLY))) { continue; }
        __int8 i8Merit = GetMediaTypeMerit(m_pMixerType);
        if (!i8Merit) { continue; }// reject types set to 0 in the list

        // construct the list
        size_t upInsertPos = 0;
        ptrdiff_t i = paValidMixerTypes.GetCount();
        while (--i >= 0) {// the first item will set -1 here
            __int8 i8ThisMerit = GetMediaTypeMerit(paValidMixerTypes[i]);
            if (i8Merit < i8ThisMerit) {
                upInsertPos = i;
                break;
            } else {
                upInsertPos = i + 1;
            }
        }
        paValidMixerTypes.InsertAt(upInsertPos, m_pMixerType);
    }

    // Step 4. Adjust the mixer's type to match our requirements
    ptrdiff_t i = paValidMixerTypes.GetCount() - 1;
    if (i < 0) {
        ASSERT(0);
        return E_FAIL;
    }
#ifdef _DEBUG
    do {
        if (SUCCEEDED(paValidMixerTypes[i]->GetGUID(MF_MT_SUBTYPE, &guidSubType))) {
            TRACE(L"EVR: Valid mixer output type: %s\n", GetSurfaceFormatName(guidSubType.Data1));
        }
    } while (--i >= 0);
    i = paValidMixerTypes.GetCount() - 1;
#endif
    do {
        m_pMixerType = paValidMixerTypes[i];
#ifdef _DEBUG
        if (SUCCEEDED(m_pMixerType->GetGUID(MF_MT_SUBTYPE, &guidSubType))) {
            TRACE(L"EVR: Trying mixer output type: %s\n", GetSurfaceFormatName(guidSubType.Data1));
        }
#endif
        // Step 5. Try to set the media type on ourselves
        if (FAILED(hr = InitializeDevice())) {
            m_pMixerType = nullptr;// no reference was added yet
            continue;
        }

        // Step 6. Set output media type on mixer
        if (FAILED(hr = m_pMixer->SetOutputType(0, m_pMixerType, 0))
                || FAILED(m_pMixerType->GetGUID(MF_MT_SUBTYPE, &guidSubType))) {
            m_pMixer->SetOutputType(0, nullptr, 0);
            m_pMixerType = nullptr;// no reference was added yet
            --i;
            continue;
        }

        //      SetChromaType(guidSubType.Data1);
        m_strStatsMsg[MSG_MIXEROUT] = L"Mixer format : Input ";
        m_strStatsMsg[MSG_MIXEROUT] += GetSurfaceFormatName(guidSubType.Data1);
        m_strStatsMsg[MSG_MIXEROUT] += L", Output ";// note: this line is completed at the stats streen constructor
        m_strStatsMsg[MSG_MIXEROUT] += GetSurfaceFormatName(m_dfSurfaceType);
        m_pMixerType->AddRef();
        break;
    } while (--i >= 0);

    //  m_llTimePerFrame = 0;// reset the data afterwards
    return hr;
}

bool CSyncAP::GetSampleFromMixer()
{
    MFT_OUTPUT_DATA_BUFFER Buffer;
    HRESULT hr = S_OK;
    DWORD dwStatus;
    LONGLONG llClockBefore = 0;
    LONGLONG llClockAfter  = 0;
    LONGLONG llMixerLatency;

    UINT dwSurface;
    bool newSample = false;

    while (SUCCEEDED(hr)) { // Get as many frames as there are and that we have samples for
        CComPtr<IMFSample> pSample;
        CComPtr<IMFSample> pNewSample;
        if (FAILED(GetFreeSample(&pSample))) { // All samples are taken for the moment. Better luck next time
            break;
        }

        memset(&Buffer, 0, sizeof(Buffer));
        Buffer.pSample = pSample;
        pSample->GetUINT32(GUID_SURFACE_INDEX, &dwSurface);
        {
            llClockBefore = PerfCounter100ns();
            hr = m_pMixer->ProcessOutput(0 , 1, &Buffer, &dwStatus);
            llClockAfter = PerfCounter100ns();
        }

        if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) { // There are no samples left in the mixer
            MoveToFreeList(pSample, false);
            break;
        }
        if (m_pSink) {
            llMixerLatency = llClockAfter - llClockBefore;
            m_pSink->Notify(EC_PROCESSING_LATENCY, (LONG_PTR)&llMixerLatency, 0);
        }

        newSample = true;

        if (GetRenderersData()->m_fTearingTest) {
            RECT rcTearing;

            rcTearing.left = m_ipTearingPos;
            rcTearing.top = 0;
            rcTearing.right = rcTearing.left + 4;
            rcTearing.bottom = m_u32VideoHeight;
            m_pD3DDev->ColorFill(m_apVideoSurface[dwSurface], &rcTearing, D3DCOLOR_ARGB(255, 255, 0, 0));

            rcTearing.left = (rcTearing.right + 15) % m_u32VideoWidth;
            rcTearing.right = rcTearing.left + 4;
            m_pD3DDev->ColorFill(m_apVideoSurface[dwSurface], &rcTearing, D3DCOLOR_ARGB(255, 255, 0, 0));
            m_ipTearingPos = (m_ipTearingPos + 7) % m_u32VideoWidth;
        }
        MoveToScheduledList(pSample, false); // Schedule, then go back to see if there is more where that came from
    }
    return newSample;
}

STDMETHODIMP CSyncAP::GetCurrentMediaType(__deref_out IMFVideoMediaType** ppMediaType)
{
    if (!m_pMixerType) { return MF_E_NOT_INITIALIZED; }
    if (!ppMediaType) { return E_POINTER; }
    HRESULT hr;
    if (FAILED(hr = CheckShutdown())) { return hr; }

    return m_pMixerType->QueryInterface(IID_IMFVideoMediaType, reinterpret_cast<void**>(&ppMediaType));
}

// IMFTopologyServiceLookupClient
STDMETHODIMP CSyncAP::InitServicePointers(__in IMFTopologyServiceLookup* pLookup)
{
    HRESULT hr;
    DWORD dwObjects = 1;
    hr = pLookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0, MR_VIDEO_MIXER_SERVICE, IID_IMFTransform, (void**)&m_pMixer, &dwObjects);
    hr = pLookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0, MR_VIDEO_RENDER_SERVICE, IID_IMediaEventSink, (void**)&m_pSink, &dwObjects);
    hr = pLookup->LookupService(MF_SERVICE_LOOKUP_GLOBAL, 0, MR_VIDEO_RENDER_SERVICE, IID_IMFClock, (void**)&m_pClock, &dwObjects);
    StartWorkerThreads();
    return S_OK;
}

STDMETHODIMP CSyncAP::ReleaseServicePointers()
{
    StopWorkerThreads();
    if (m_pMixer) {
        m_pMixer->Release();// the external EVR will keep hold of references to the mixer and sink
        m_pMixer = nullptr;
    }
    if (m_pSink) {
        m_pSink->Release();
        m_pSink = nullptr;
    }
    ULONG u;
    if (m_pClock) {
        u = m_pClock->Release();
        ASSERT(!u);
        m_pClock = nullptr;
    }
    return S_OK;
}

// IMFVideoDeviceID
STDMETHODIMP CSyncAP::GetDeviceID(__out  IID* pDeviceID)
{
    if (!pDeviceID) { return E_POINTER; }
    *pDeviceID = IID_IDirect3DDevice9;
    return S_OK;
}

// IMFGetService
STDMETHODIMP CSyncAP::GetService(__RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject)
{
    if (guidService == MR_VIDEO_RENDER_SERVICE) {
        return QueryInterface(riid, ppvObject);
    } else if (guidService == MR_VIDEO_ACCELERATION_SERVICE) {
        return m_pD3DManager->QueryInterface(IID_IDirect3DDeviceManager9, (void**)ppvObject);
    }

    return E_NOINTERFACE;
}

// IMFAsyncCallback
STDMETHODIMP CSyncAP::GetParameters(__RPC__out DWORD* pdwFlags, __RPC__out DWORD* pdwQueue)
{
    return E_NOTIMPL;
}

STDMETHODIMP CSyncAP::Invoke(__RPC__in_opt IMFAsyncResult* pAsyncResult)
{
    return E_NOTIMPL;
}

// IMFVideoDisplayControl
STDMETHODIMP CSyncAP::GetNativeVideoSize(SIZE* pszVideo, SIZE* pszARVideo)
{
    if (pszVideo) {
        pszVideo->cx    = m_u32VideoWidth;
        pszVideo->cy    = m_u32VideoHeight;
    }
    if (pszARVideo) {
        pszARVideo->cx  = m_u32AspectRatioWidth;
        pszARVideo->cy  = m_u32AspectRatioHeight;
    }
    return S_OK;
}

STDMETHODIMP CSyncAP::GetIdealVideoSize(SIZE* pszMin, SIZE* pszMax)
{
    if (pszMin) {
        pszMin->cx = 1;
        pszMin->cy = 1;
    }
    if (pszMax) {
        pszMax->cx = m_u32WindowWidth;
        pszMax->cy = m_u32WindowHeight;
    }
    return S_OK;
}

STDMETHODIMP CSyncAP::SetVideoPosition(const MFVideoNormalizedRect* pnrcSource, const LPRECT prcDest)
{
    return S_OK;
}

STDMETHODIMP CSyncAP::GetVideoPosition(MFVideoNormalizedRect* pnrcSource, LPRECT prcDest)
{
    if (pnrcSource) {
        pnrcSource->left    = 0.0;
        pnrcSource->top     = 0.0;
        pnrcSource->right   = 1.0;
        pnrcSource->bottom  = 1.0;
    }
    if (prcDest) {
        memcpy(prcDest, &m_VideoRect, sizeof(m_VideoRect));     //GetClientRect (m_hVideoWnd, prcDest);
    }
    return S_OK;
}

STDMETHODIMP CSyncAP::SetAspectRatioMode(DWORD dwAspectRatioMode)
{
    return S_OK;
}

STDMETHODIMP CSyncAP::GetAspectRatioMode(DWORD* pdwAspectRatioMode)
{
    if (!pdwAspectRatioMode) { return E_POINTER; }
    *pdwAspectRatioMode = MFVideoARMode_PreservePicture;
    return S_OK;
}

STDMETHODIMP CSyncAP::SetVideoWindow(HWND hwndVideo)
{
    ASSERT(m_hVideoWnd == hwndVideo);
    return S_OK;
}

STDMETHODIMP CSyncAP::GetVideoWindow(HWND* phwndVideo)
{
    if (!phwndVideo) { return E_POINTER; }
    *phwndVideo = m_hVideoWnd;
    return S_OK;
}

STDMETHODIMP CSyncAP::RepaintVideo()
{
    Paint(true);
    return S_OK;
}

STDMETHODIMP CSyncAP::GetCurrentImage(BITMAPINFOHEADER* pBih, BYTE** pDib, DWORD* pcbDib, LONGLONG* pTimeStamp)
{
    ASSERT(FALSE);
    return E_NOTIMPL;
}

STDMETHODIMP CSyncAP::SetBorderColor(COLORREF Clr)
{
    m_BorderColor = Clr;
    return S_OK;
}

STDMETHODIMP CSyncAP::GetBorderColor(COLORREF* pClr)
{
    if (!pClr) { return E_POINTER; }
    *pClr = m_BorderColor;
    return S_OK;
}

STDMETHODIMP CSyncAP::SetRenderingPrefs(DWORD dwRenderFlags)
{
    m_dwVideoRenderPrefs = (MFVideoRenderPrefs)dwRenderFlags;
    return S_OK;
}

STDMETHODIMP CSyncAP::GetRenderingPrefs(DWORD* pdwRenderFlags)
{
    if (!pdwRenderFlags) { return E_POINTER; }
    *pdwRenderFlags = m_dwVideoRenderPrefs;
    return S_OK;
}

STDMETHODIMP CSyncAP::SetFullscreen(BOOL fFullscreen)
{
    ASSERT(FALSE);
    return E_NOTIMPL;
}

STDMETHODIMP CSyncAP::GetFullscreen(BOOL* pfFullscreen)
{
    ASSERT(FALSE);
    return E_NOTIMPL;
}

// IEVRTrustedVideoPlugin
STDMETHODIMP CSyncAP::IsInTrustedVideoMode(BOOL* pYes)
{
    if (!pYes) { return E_POINTER; }
    *pYes = TRUE;
    return S_OK;
}

STDMETHODIMP CSyncAP::CanConstrict(BOOL* pYes)
{
    if (!pYes) { return E_POINTER; }
    *pYes = TRUE;
    return S_OK;
}

STDMETHODIMP CSyncAP::SetConstriction(DWORD dwKPix)
{
    return S_OK;
}

STDMETHODIMP CSyncAP::DisableImageExport(BOOL bDisable)
{
    return S_OK;
}

// IDirect3DDeviceManager9
STDMETHODIMP CSyncAP::ResetDevice(IDirect3DDevice9* pDevice, UINT resetToken)
{
    HRESULT     hr = m_pD3DManager->ResetDevice(pDevice, resetToken);
    return hr;
}

STDMETHODIMP CSyncAP::OpenDeviceHandle(HANDLE* phDevice)
{
    HRESULT     hr = m_pD3DManager->OpenDeviceHandle(phDevice);
    return hr;
}

STDMETHODIMP CSyncAP::CloseDeviceHandle(HANDLE hDevice)
{
    HRESULT     hr = m_pD3DManager->CloseDeviceHandle(hDevice);
    return hr;
}

STDMETHODIMP CSyncAP::TestDevice(HANDLE hDevice)
{
    HRESULT     hr = m_pD3DManager->TestDevice(hDevice);
    return hr;
}

STDMETHODIMP CSyncAP::LockDevice(HANDLE hDevice, IDirect3DDevice9** ppDevice, BOOL fBlock)
{
    HRESULT     hr = m_pD3DManager->LockDevice(hDevice, ppDevice, fBlock);
    return hr;
}

STDMETHODIMP CSyncAP::UnlockDevice(HANDLE hDevice, BOOL fSaveState)
{
    HRESULT     hr = m_pD3DManager->UnlockDevice(hDevice, fSaveState);
    return hr;
}

STDMETHODIMP CSyncAP::GetVideoService(HANDLE hDevice, REFIID riid, void** ppService)
{
    HRESULT     hr = m_pD3DManager->GetVideoService(hDevice, riid, ppService);

    if (riid == IID_IDirectXVideoDecoderService) {
        UINT        nNbDecoder = 5;
        GUID*       pDecoderGuid;
        IDirectXVideoDecoderService*        pDXVAVideoDecoder = (IDirectXVideoDecoderService*) *ppService;
        pDXVAVideoDecoder->GetDecoderDeviceGuids(&nNbDecoder, &pDecoderGuid);
    } else if (riid == IID_IDirectXVideoProcessorService) {
        IDirectXVideoProcessorService*      pDXVAProcessor = (IDirectXVideoProcessorService*) *ppService;
        UNREFERENCED_PARAMETER(pDXVAProcessor);
    }

    return hr;
}

STDMETHODIMP CSyncAP::GetNativeVideoSize(LONG* lpWidth, LONG* lpHeight, LONG* lpARWidth, LONG* lpARHeight)
{
    // This function should be called...
    ASSERT(FALSE);

    if (lpWidth) {
        *lpWidth    = m_u32VideoWidth;
    }
    if (lpHeight)    {
        *lpHeight   = m_u32VideoHeight;
    }
    if (lpARWidth)   {
        *lpARWidth  = m_u32AspectRatioWidth;
    }
    if (lpARHeight)  {
        *lpARHeight = m_u32AspectRatioHeight;
    }
    return S_OK;
}

HRESULT CSyncAP::InitializeDevice()
{
    HRESULT hr;
    CAutoLock lock2(&m_ImageProcessingLock);
    CAutoLock cRenderLock(&m_csAllocatorLock);

    RemoveAllSamples();
    DeleteSurfaces();

    // Retrieve the video attributes
    __declspec(align(8)) UINT32 pVSize[2];// re-used a few times
    if (FAILED(hr = m_pMixerType->GetUINT32(MF_MT_INTERLACE_MODE, pVSize))) { return hr; }
    m_upInterlaced = *pVSize != MFVideoInterlace_Progressive;

    //hr = MFGetAttributeSize(m_pMixerType, MF_MT_FRAME_RATE, &uiFRn, &uiFRd); doesn't work correctly for interlaced types

    if (FAILED(hr = MFGetAttributeSize(m_pMixerType, MF_MT_FRAME_SIZE, &pVSize[0], &pVSize[1]))) { return hr; }
    // video size parts
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
    __m128 x2 = _mm_set_ps1(1.0f);
    __m128i xVS = _mm_loadl_epi64(reinterpret_cast<__m128i*>(pVSize));
    __m128d x0 = _mm_cvtepi32_pd(xVS);// __int32 to double
    _mm_storel_epi64(reinterpret_cast<__m128i*>(&m_u32VideoWidth), xVS);// also stores m_u32VideoHeight
    __m128 x1 = _mm_cvtpd_ps(x0);// double to float
    _mm_store_pd(&m_dVideoWidth, x0);;// also stores m_dVideoHeight
    x2 = _mm_div_ps(x2, x1);// reciprocal trough _mm_rcp_ps() isn't accurate
    _mm_storel_pi(reinterpret_cast<__m64*>(&m_fVideoWidth), x1);// not an MMX function, also stores m_fVideoHeight
    _mm_storel_pi(reinterpret_cast<__m64*>(&m_fVideoWidthr), x2);// not an MMX function, also stores m_fVideoHeightr
#else
    m_u32VideoWidth = pVSize[0];
    m_u32VideoHeight = pVSize[1];
    m_dVideoWidth = static_cast<double>(static_cast<__int32>(m_u32VideoWidth));// the standard converter only does a proper job with signed values
    m_dVideoHeight = static_cast<double>(static_cast<__int32>(m_u32VideoHeight));
    m_fVideoWidth = static_cast<float>(m_dVideoWidth);
    m_fVideoHeight = static_cast<float>(m_dVideoHeight);
    m_fVideoWidthr = 1.0 / m_fVideoWidth;
    m_fVideoHeightr = 1.0 / m_fVideoHeight;
#endif

    if (FAILED(hr = MFGetAttributeSize(m_pMixerType, MF_MT_PIXEL_ASPECT_RATIO, &pVSize[0], &pVSize[1]))) {
        return hr;
    }
    UINT32 uiARx = m_u32VideoWidth * pVSize[0];
    UINT32 uiARy = m_u32VideoHeight * pVSize[1];
    if (uiARx && uiARy) { // if either of these is 0, it will get stuck into an infinite loop
        // division reduction
        UINT32 a = uiARx, b = uiARy;
        do {
            UINT32 tmp = a;
            a = b % tmp;
            b = tmp;
        } while (a);
        uiARx /= b;
        uiARy /= b;
    }
    m_u32AspectRatioWidth = uiARx;
    m_u32AspectRatioHeight = uiARy;

    if (FAILED(hr = AllocSurfaces(m_upHighColorResolution ? D3DFMT_A2R10G10B10 : D3DFMT_X8R8G8B8))) { return hr; }
    for (size_t i = 0; i < m_nDXSurface; ++i) {
        CComPtr<IMFSample> pMFSample;
        if (FAILED(hr = pfMFCreateVideoSampleFromSurface(m_apVideoSurface[i], &pMFSample))) { return hr; }
        pMFSample->SetUINT32(GUID_SURFACE_INDEX, i);
        m_FreeSamples.AddTail(pMFSample);
    }

    Paint(false);// helps the renderer to initialize
    return S_OK;
}

DWORD WINAPI CSyncAP::MixerThreadStatic(LPVOID lpParam)
{
    DEBUG_ONLY(SetThreadName(0xFFFFFFFF, "CSyncAP::MixerThread"));
    CSyncAP* pThis = (CSyncAP*) lpParam;
    pThis->MixerThread();
    return 0;
}

void CSyncAP::MixerThread()
{
    HANDLE hEvts[] = {m_hEvtQuit};
    bool bQuit = false;

    while (!bQuit) {
        DWORD dwObject = WaitForMultipleObjects(_countof(hEvts), hEvts, FALSE, 1);
        switch (dwObject) {
            case WAIT_OBJECT_0 :
                bQuit = true;
                break;
            case WAIT_TIMEOUT : {
                bool bNewSample = false;
                {
                    CAutoLock AutoLock(&m_ImageProcessingLock);
                    bNewSample = GetSampleFromMixer();
                }
                if (m_bUseInternalTimer) {
                    m_pSubPicQueue->SetFPS(m_dDetectedVideoFrameRate);
                }
            }
            break;
        }
    }
}

DWORD WINAPI CSyncAP::RenderThreadStatic(LPVOID lpParam)
{
    DEBUG_ONLY(SetThreadName(0xFFFFFFFF, "CSyncAP::RenderThread"));
    CSyncAP* pThis = (CSyncAP*)lpParam;
    pThis->RenderThread();
    return 0;
}

// Get samples that have been received and queued up by MixerThread() and present them at the correct time by calling Paint().
void CSyncAP::RenderThread()
{
    HANDLE hEvts[] = {m_hEvtQuit, m_hEvtFlush, m_hEvtSkip};
    bool bQuit = false;

    LONGLONG llRefClockTime;
    double dTargetSyncOffset;
    MFTIME llSystemTime;

    DWORD dwObject;
    int nSamplesLeft;
    CComPtr<IMFSample> pNewSample = nullptr; // The sample next in line to be presented

    // Tell Vista Multimedia Class Scheduler we are doing threaded playback (increase priority)
    HANDLE hAvrt = nullptr;
    if (pfAvSetMmThreadCharacteristicsW) {
        DWORD dwTaskIndex = 0;
        hAvrt = pfAvSetMmThreadCharacteristicsW(L"Playback", &dwTaskIndex);
        if (pfAvSetMmThreadPriority) {
            pfAvSetMmThreadPriority(hAvrt, AVRT_PRIORITY_HIGH);
        }
    }

    pNewSample = nullptr;

    while (!bQuit) {
        m_lNextSampleWait = 1; // Default value for running this loop
        nSamplesLeft = 0;
        bool stepForward = false;
        LONG lDisplayCycle = (LONG)(GetDisplayCycle());
        LONG lDisplayCycle2 = (LONG)(GetDisplayCycle() / 2.0); // These are a couple of empirically determined constants used the control the "snap" function
        LONG lDisplayCycle4 = (LONG)(GetDisplayCycle() / 4.0);

        dTargetSyncOffset = mk_pRendererSettings->fTargetSyncOffset;

        if ((m_nRenderState == Started || !m_bPrerolled) && !pNewSample) { // If either streaming or the pre-roll sample and no sample yet fetched
            if (SUCCEEDED(GetScheduledSample(&pNewSample, nSamplesLeft))) { // Get the next sample
                m_llLastSampleTime = m_llSampleTime;
                if (!m_bPrerolled) {
                    m_bPrerolled = true; // m_bPrerolled is a ticket to show one (1) frame immediately
                    m_lNextSampleWait = 0; // Present immediately
                } else if (SUCCEEDED(pNewSample->GetSampleTime(&m_llSampleTime))) { // Get zero-based sample due time
                    if (m_llLastSampleTime == m_llSampleTime) { // In the rare case there are duplicate frames in the movie. There really shouldn't be but it happens.
                        MoveToFreeList(pNewSample, true);
                        pNewSample = nullptr;
                        m_lNextSampleWait = 0;
                    } else {
                        m_pClock->GetCorrelatedTime(0, &llRefClockTime, &llSystemTime); // Get zero-based reference clock time. llSystemTime is not used for anything here
                        m_lNextSampleWait = (LONG)((m_llSampleTime - llRefClockTime) / 10000); // Time left until sample is due, in ms
                        if (m_bStepping) {
                            m_lNextSampleWait = 0;
                        } else if (mk_pRendererSettings->bSynchronizeNearest) { // Present at the closest "safe" occasion at dTargetSyncOffset ms before vsync to avoid tearing
                            if (m_lNextSampleWait < -lDisplayCycle) { // We have to allow slightly negative numbers at this stage. Otherwise we get "choking" when frame rate > refresh rate
                                SetEvent(m_hEvtSkip);
                                m_bEvtSkip = true;
                            }
                            REFERENCE_TIME rtRefClockTimeNow;
                            if (m_pRefClock) {
                                m_pRefClock->GetTime(&rtRefClockTimeNow);    // Reference clock time now
                            }
                            LONG lLastVsyncTime = (LONG)((m_llEstVBlankTime - rtRefClockTimeNow) / 10000); // Last vsync time relative to now
                            if (abs(lLastVsyncTime) > lDisplayCycle) {
                                lLastVsyncTime = - lDisplayCycle;    // To even out glitches in the beginning
                            }

                            LONGLONG llNextSampleWait = (LONGLONG)(((double)lLastVsyncTime + GetDisplayCycle() - dTargetSyncOffset) * 10000); // Time from now util next safe time to Paint()
                            while ((llRefClockTime + llNextSampleWait) < (m_llSampleTime + m_llHysteresis)) { // While the proposed time is in the past of sample presentation time
                                llNextSampleWait = llNextSampleWait + (LONGLONG)(GetDisplayCycle() * 10000); // Try the next possible time, one display cycle ahead
                            }
                            m_lNextSampleWait = (LONG)(llNextSampleWait / 10000);
                            m_lShiftToNearestPrev = m_lShiftToNearest;
                            m_lShiftToNearest = (LONG)((llRefClockTime + llNextSampleWait - m_llSampleTime) / 10000); // The adjustment made to get to the sweet point in time, in ms

                            // If m_lShiftToNearest is pushed a whole cycle into the future, then we are getting more frames
                            // than we can chew and we need to throw one away. We don't want to wait many cycles and skip many
                            // frames.
                            if (m_lShiftToNearest > (lDisplayCycle + 1)) {
                                SetEvent(m_hEvtSkip);
                                m_bEvtSkip = true;
                            }

                            // We need to add a hysteresis to the control of the timing adjustment to avoid judder when
                            // presentation time is close to vsync and the renderer couldn't otherwise make up its mind
                            // whether to present before the vsync or after. That kind of indecisiveness leads to judder.
                            if (m_bSnapToVSync) {

                                if ((m_lShiftToNearestPrev - m_lShiftToNearest) > lDisplayCycle2) { // If a step down in the m_lShiftToNearest function. Display slower than video.
                                    m_bVideoSlowerThanDisplay = false;
                                    m_llHysteresis = -(LONGLONG)(10000 * lDisplayCycle4);
                                } else if ((m_lShiftToNearest - m_lShiftToNearestPrev) > lDisplayCycle2) { // If a step up
                                    m_bVideoSlowerThanDisplay = true;
                                    m_llHysteresis = (LONGLONG)(10000 * lDisplayCycle4);
                                } else if ((m_lShiftToNearest < (3 * lDisplayCycle4)) && (m_lShiftToNearest > lDisplayCycle4)) {
                                    m_llHysteresis = 0;    // Reset when between 1/4 and 3/4 of the way either way
                                }

                                if ((m_lShiftToNearest < lDisplayCycle2) && (m_llHysteresis > 0)) {
                                    m_llHysteresis = 0;    // Should never really be in this territory.
                                }
                                if (m_lShiftToNearest < 0) {
                                    m_llHysteresis = 0;    // A glitch might get us to a sticky state where both these numbers are negative.
                                }
                                if ((m_lShiftToNearest > lDisplayCycle2) && (m_llHysteresis < 0)) {
                                    m_llHysteresis = 0;
                                }
                            }
                        }

                        if (m_lNextSampleWait < 0) { // Skip late or duplicate sample.
                            SetEvent(m_hEvtSkip);
                            m_bEvtSkip = true;
                        }

                        if (m_lNextSampleWait > 1000) {
                            m_lNextSampleWait = 1000; // So as to avoid full a full stop when sample is far in the future (shouldn't really happen).
                        }
                    }
                } // if got new sample
            }
        }
        // Wait for the next presentation time (m_lNextSampleWait) or some other event.
        dwObject = WaitForMultipleObjects(_countof(hEvts), hEvts, FALSE, (DWORD)m_lNextSampleWait);
        switch (dwObject) {
            case WAIT_OBJECT_0: // Quit
                bQuit = true;
                break;

            case WAIT_OBJECT_0 + 1: // Flush
                if (pNewSample) {
                    MoveToFreeList(pNewSample, true);
                }
                pNewSample = nullptr;
                FlushSamples();
                m_bEvtFlush = false;
                ResetEvent(m_hEvtFlush);
                m_bPrerolled = false;
                m_lShiftToNearest = 0;
                stepForward = true;
                break;

            case WAIT_OBJECT_0 + 2: // Skip sample
                m_pcFramesDropped++;
                m_llSampleTime = m_llLastSampleTime; // This sample will never be shown
                m_bEvtSkip = false;
                ResetEvent(m_hEvtSkip);
                stepForward = true;
                break;

            case WAIT_TIMEOUT: // Time to show the sample or something
                if (m_bPendingRenegotiate) {
                    if (pNewSample) {
                        MoveToFreeList(pNewSample, true);
                    }
                    pNewSample = nullptr;
                    FlushSamples();
                    RenegotiateMediaType();
                    m_bPendingRenegotiate = false;
                }

                if (m_bPendingResetDevice) {
                    if (pNewSample) {
                        MoveToFreeList(pNewSample, true);
                    }
                    pNewSample = nullptr;
                    SendResetRequest();
                } else if (m_nStepCount < 0) {
                    m_nStepCount = 0;
                    m_pcFramesDropped++;
                    stepForward = true;
                } else if (pNewSample && (m_nStepCount > 0)) {
                    pNewSample->GetUINT32(GUID_SURFACE_INDEX, (UINT32*)&m_nCurSurface);
                    if (!g_bExternalSubtitleTime) {
                        CSubPicAllocatorPresenterImpl::SetTime(g_tSegmentStart + m_llSampleTime);
                    }
                    Paint(true);
                    CompleteFrameStep(false);
                    m_pcFramesDrawn++;
                    stepForward = true;
                } else if (pNewSample && !m_bStepping) { // When a stepped frame is shown, a new one is fetched that we don't want to show here while stepping
                    pNewSample->GetUINT32(GUID_SURFACE_INDEX, (UINT32*)&m_nCurSurface);
                    if (!g_bExternalSubtitleTime) {
                        CSubPicAllocatorPresenterImpl::SetTime(g_tSegmentStart + m_llSampleTime);
                    }
                    Paint(true);
                    m_pcFramesDrawn++;
                    stepForward = true;
                }
                break;
        } // switch
        if (pNewSample && stepForward) {
            MoveToFreeList(pNewSample, true);
            pNewSample = nullptr;
        }
    } // while

    if (pNewSample) {
        MoveToFreeList(pNewSample, true);
        pNewSample = nullptr;
    }

    if (pfAvRevertMmThreadCharacteristics) {
        pfAvRevertMmThreadCharacteristics(hAvrt);
    }
}

__declspec(nothrow noalias) void CSyncAP::ResetDevice()
{
    CAutoLock lock2(&m_ImageProcessingLock);
    CAutoLock cRenderLock(&m_csAllocatorLock);
    RemoveAllSamples();

    ResetLocalDevice();

    for (size_t i = 0; i < m_nDXSurface; ++i) {
        CComPtr<IMFSample> pMFSample;
        HRESULT hr = pfMFCreateVideoSampleFromSurface(m_apVideoSurface[i], &pMFSample);
        if (SUCCEEDED(hr)) {
            pMFSample->SetUINT32(GUID_SURFACE_INDEX, i);
            m_FreeSamples.AddTail(pMFSample);
        } else {
            ASSERT(0);
        }
    }
}

void CSyncAP::OnResetDevice()
{
    TRACE(L"--> CSyncAP::OnResetDevice on thread: %d\n", GetCurrentThreadId());
    HRESULT hr;
    hr = m_pD3DManager->ResetDevice(m_pD3DDev, m_nResetToken);
    if (m_pSink) {
        m_pSink->Notify(EC_VIDEO_SIZE_CHANGED, MAKELPARAM(m_u32VideoWidth, m_u32VideoHeight), 0);
    }
}

void CSyncAP::RemoveAllSamples()
{
    CAutoLock AutoLock(&m_ImageProcessingLock);
    FlushSamples();
    m_ScheduledSamples.RemoveAll();
    m_FreeSamples.RemoveAll();
    m_nUsedBuffer = 0;
}

HRESULT CSyncAP::GetFreeSample(IMFSample** ppSample)
{
    CAutoLock lock(&m_SampleQueueLock);
    HRESULT     hr = S_OK;

    if (m_FreeSamples.GetCount() > 1) { // Cannot use first free buffer (can be currently displayed)
        InterlockedIncrement(&m_nUsedBuffer);
        *ppSample = m_FreeSamples.RemoveHead().Detach();
    } else {
        hr = MF_E_SAMPLEALLOCATOR_EMPTY;
    }

    return hr;
}

HRESULT CSyncAP::GetScheduledSample(IMFSample** ppSample, int& _Count)
{
    CAutoLock lock(&m_SampleQueueLock);
    HRESULT     hr = S_OK;

    _Count = m_ScheduledSamples.GetCount();
    if (_Count > 0) {
        *ppSample = m_ScheduledSamples.RemoveHead().Detach();
        --_Count;
    } else {
        hr = MF_E_SAMPLEALLOCATOR_EMPTY;
    }

    return hr;
}

void CSyncAP::MoveToFreeList(IMFSample* pSample, bool bTail)
{
    CAutoLock lock(&m_SampleQueueLock);
    InterlockedDecrement(&m_nUsedBuffer);
    if (m_bPendingMediaFinished && m_nUsedBuffer == 0) {
        m_bPendingMediaFinished = false;
        m_pSink->Notify(EC_COMPLETE, 0, 0);
    }
    if (bTail) {
        m_FreeSamples.AddTail(pSample);
    } else {
        m_FreeSamples.AddHead(pSample);
    }
}

void CSyncAP::MoveToScheduledList(IMFSample* pSample, bool _bSorted)
{
    if (_bSorted) {
        CAutoLock lock(&m_SampleQueueLock);
        m_ScheduledSamples.AddHead(pSample);
    } else {
        CAutoLock lock(&m_SampleQueueLock);
        m_ScheduledSamples.AddTail(pSample);
    }
}

void CSyncAP::FlushSamples()
{
    CAutoLock lock2(&m_SampleQueueLock);
    FlushSamplesInternal();
}

void CSyncAP::FlushSamplesInternal()
{
    m_bPrerolled = false;
    while (m_ScheduledSamples.GetCount() > 0) {
        CComPtr<IMFSample> pMFSample;
        pMFSample = m_ScheduledSamples.RemoveHead();
        MoveToFreeList(pMFSample, true);
    }
}

__declspec(nothrow noalias) void CSyncAP::AdviseSyncClock(__inout ISyncClock* sC)
{
    m_pGenlock->AdviseSyncClock(sC);
}

HRESULT CSyncAP::BeginStreaming()
{
    m_pcFramesDropped = 0;
    m_pcFramesDrawn = 0;

    CComPtr<IBaseFilter> pEVR;
    FILTER_INFO filterInfo;
    ZeroMemory(&filterInfo, sizeof(filterInfo));
    m_pOuterEVR->QueryInterface(IID_IBaseFilter, (void**)&pEVR);
    pEVR->QueryFilterInfo(&filterInfo); // This addref's the pGraph member

    BeginEnumFilters(filterInfo.pGraph, pEF, pBF)
    if (CComQIPtr<IAMAudioRendererStats> pAS = pBF) {
        m_pAudioStats = pAS;
    };
    EndEnumFilters

    m_pRefClock = nullptr;
    pEVR->GetSyncSource(&m_pRefClock);
    if (filterInfo.pGraph) {
        filterInfo.pGraph->Release();
    }
    m_pGenlock->SetMonitor(GetAdapter(m_pD3D));
    m_pGenlock->GetTiming();

    ResetStats();
    EstimateRefreshTimings();
    if (m_dFrameCycle > 0.0) {
        m_dCycleDifference = GetCycleDifference();    // Might have moved to another display
    }
    return S_OK;
}

CSyncRenderer::CSyncRenderer(const TCHAR* pName, LPUNKNOWN pUnk, HRESULT& hr, VMR9AlphaBitmap* pVMR9AlphaBitmap, CSyncAP* pAllocatorPresenter): CUnknown(pName, pUnk)
{
    hr = m_pEVR.CoCreateInstance(CLSID_EnhancedVideoRenderer, GetOwner());
    m_pVMR9AlphaBitmap = pVMR9AlphaBitmap;
    m_pAllocatorPresenter = pAllocatorPresenter;
}

CSyncRenderer::~CSyncRenderer()
{
}

HRESULT STDMETHODCALLTYPE CSyncRenderer::GetState(DWORD dwMilliSecsTimeout, __out  FILTER_STATE* State)
{
    CComPtr<IBaseFilter> pEVRBase;
    if (m_pEVR) {
        m_pEVR->QueryInterface(&pEVRBase);
    }
    if (pEVRBase) {
        return pEVRBase->GetState(dwMilliSecsTimeout, State);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::EnumPins(__out IEnumPins** ppEnum)
{
    CComPtr<IBaseFilter> pEVRBase;
    if (m_pEVR) {
        m_pEVR->QueryInterface(&pEVRBase);
    }
    if (pEVRBase) {
        return pEVRBase->EnumPins(ppEnum);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::FindPin(LPCWSTR Id, __out  IPin** ppPin)
{
    CComPtr<IBaseFilter> pEVRBase;
    if (m_pEVR) {
        m_pEVR->QueryInterface(&pEVRBase);
    }
    if (pEVRBase) {
        return pEVRBase->FindPin(Id, ppPin);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::QueryFilterInfo(__out  FILTER_INFO* pInfo)
{
    CComPtr<IBaseFilter> pEVRBase;
    if (m_pEVR) {
        m_pEVR->QueryInterface(&pEVRBase);
    }
    if (pEVRBase) {
        return pEVRBase->QueryFilterInfo(pInfo);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::JoinFilterGraph(__in_opt  IFilterGraph* pGraph, __in_opt  LPCWSTR pName)
{
    CComPtr<IBaseFilter> pEVRBase;
    if (m_pEVR) {
        m_pEVR->QueryInterface(&pEVRBase);
    }
    if (pEVRBase) {
        return pEVRBase->JoinFilterGraph(pGraph, pName);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::QueryVendorInfo(__out  LPWSTR* pVendorInfo)
{
    CComPtr<IBaseFilter> pEVRBase;
    if (m_pEVR) {
        m_pEVR->QueryInterface(&pEVRBase);
    }
    if (pEVRBase) {
        return pEVRBase->QueryVendorInfo(pVendorInfo);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::Stop()
{
    CComPtr<IBaseFilter> pEVRBase;
    if (m_pEVR) {
        m_pEVR->QueryInterface(&pEVRBase);
    }
    if (pEVRBase) {
        return pEVRBase->Stop();
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::Pause()
{
    CComPtr<IBaseFilter> pEVRBase;
    if (m_pEVR) {
        m_pEVR->QueryInterface(&pEVRBase);
    }
    if (pEVRBase) {
        return pEVRBase->Pause();
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::Run(REFERENCE_TIME tStart)
{
    CComPtr<IBaseFilter> pEVRBase;
    if (m_pEVR) {
        m_pEVR->QueryInterface(&pEVRBase);
    }
    if (pEVRBase) {
        return pEVRBase->Run(tStart);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::SetSyncSource(__in_opt IReferenceClock* pClock)
{
    CComPtr<IBaseFilter> pEVRBase;
    if (m_pEVR) {
        m_pEVR->QueryInterface(&pEVRBase);
    }
    if (pEVRBase) {
        return pEVRBase->SetSyncSource(pClock);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::GetSyncSource(__deref_out_opt IReferenceClock** pClock)
{
    CComPtr<IBaseFilter> pEVRBase;
    if (m_pEVR) {
        m_pEVR->QueryInterface(&pEVRBase);
    }
    if (pEVRBase) {
        return pEVRBase->GetSyncSource(pClock);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::GetClassID(__RPC__out CLSID* pClassID)
{
    CComPtr<IBaseFilter> pEVRBase;
    if (m_pEVR) {
        m_pEVR->QueryInterface(&pEVRBase);
    }
    if (pEVRBase) {
        return pEVRBase->GetClassID(pClassID);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSyncRenderer::GetAlphaBitmapParameters(VMR9AlphaBitmap* pBmpParms)
{
    if (!pBmpParms) { return E_POINTER; }
    CAutoLock BitMapLock(&m_pAllocatorPresenter->m_csVMR9AlphaBitmapLock);
    memcpy(pBmpParms, m_pVMR9AlphaBitmap, sizeof(VMR9AlphaBitmap));
    return S_OK;
}

STDMETHODIMP CSyncRenderer::SetAlphaBitmap(const VMR9AlphaBitmap*  pBmpParms)
{
    if (!pBmpParms) { return E_POINTER; }
    CAutoLock BitMapLock(&m_pAllocatorPresenter->m_csVMR9AlphaBitmapLock);
    memcpy(m_pVMR9AlphaBitmap, pBmpParms, sizeof(VMR9AlphaBitmap));
    m_pVMR9AlphaBitmap->dwFlags |= VMRBITMAP_UPDATE;
    m_pAllocatorPresenter->UpdateAlphaBitmap();
    return S_OK;
}

STDMETHODIMP CSyncRenderer::UpdateAlphaBitmapParameters(const VMR9AlphaBitmap* pBmpParms)
{
    if (!pBmpParms) { return E_POINTER; }
    CAutoLock BitMapLock(&m_pAllocatorPresenter->m_csVMR9AlphaBitmapLock);
    memcpy(m_pVMR9AlphaBitmap, pBmpParms, sizeof(VMR9AlphaBitmap));
    m_pVMR9AlphaBitmap->dwFlags |= VMRBITMAP_UPDATE;
    m_pAllocatorPresenter->UpdateAlphaBitmap();
    return S_OK;
}

CGenlock::CGenlock(CRenderersSettings const* pkRendererSettings, DOUBLE target, DOUBLE limit, INT lineD, INT colD, DOUBLE clockD, UINT mon):
    mk_pRendererSettings(pkRendererSettings),
    targetSyncOffset(target), // Target sync offset, typically around 10 ms
    controlLimit(limit), // How much sync offset is allowed to drift from target sync offset before control kicks in
    lineDelta(lineD), // Number of rows used in display frequency adjustment, typically 1 (one)
    columnDelta(colD),  // Number of columns used in display frequency adjustment, typically 1 - 2
    cycleDelta(clockD),  // Delta used in clock speed adjustment. In fractions of 1.0. Typically around 0.001
    monitor(mon) // The monitor to be adjusted if the display refresh rate is the controlled parameter
{
    lowSyncOffset = targetSyncOffset - controlLimit;
    highSyncOffset = targetSyncOffset + controlLimit;
    adjDelta = 0;
    displayAdjustmentsMade = 0;
    clockAdjustmentsMade = 0;
    displayFreqCruise = 0;
    displayFreqFaster = 0;
    displayFreqSlower = 0;
    curDisplayFreq = 0;
    psWnd = nullptr;
    liveSource = FALSE;
    powerstripTimingExists = FALSE;
    syncOffsetFifo = DEBUG_NEW MovingAverage(64);
    frameCycleFifo = DEBUG_NEW MovingAverage(4);
}

CGenlock::~CGenlock()
{
    ResetTiming();
    SAFE_DELETE(syncOffsetFifo);
    SAFE_DELETE(frameCycleFifo);
    syncClock = nullptr;
};

BOOL CGenlock::PowerstripRunning()
{
    psWnd = FindWindow(_T("TPShidden"), nullptr);
    if (!psWnd) {
        return FALSE;    // Powerstrip is not running
    } else {
        return TRUE;
    }
}

// Get the display timing parameters through PowerStrip (if running).
HRESULT CGenlock::GetTiming()
{
    ATOM getTiming;
    LPARAM lParam = 0;
    WPARAM wParam = monitor;
    size_t i = 0;
    size_t j = 0;
    INT params = 0;
    TCHAR tmpStr[MAX_LOADSTRING];

    CAutoLock lock(&csGenlockLock);
    if (!PowerstripRunning()) {
        return E_FAIL;
    }

    getTiming = static_cast<ATOM>(SendMessage(psWnd, UM_GETTIMING, wParam, lParam));
    GlobalGetAtomName(getTiming, savedTiming, MAX_LOADSTRING);

    while (params < TIMING_PARAM_CNT) {
        while (savedTiming[i] != ',' && savedTiming[i] != '\0') {
            tmpStr[j++] = savedTiming[i];
            tmpStr[j] = '\0';
            i++;
        }
        i++; // Skip trailing comma
        j = 0;
        displayTiming[params] = _ttoi(tmpStr);
        displayTimingSave[params] = displayTiming[params];
        params++;
    }

    // The display update frequency is controlled by adding and subtracting pixels form the
    // image. This is done by either subtracting columns or rows or both. Some displays like
    // row adjustments and some column adjustments. One should probably not do both.
    StringCchPrintf(faster, MAX_LOADSTRING, _T("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\0"),
                    displayTiming[0],
                    displayTiming[HFRONTPORCH] - columnDelta,
                    displayTiming[2],
                    displayTiming[3],
                    displayTiming[4],
                    displayTiming[VFRONTPORCH] - lineDelta,
                    displayTiming[6],
                    displayTiming[7],
                    displayTiming[PIXELCLOCK],
                    displayTiming[9]
                   );

    // Nominal update frequency
    StringCchPrintf(cruise, MAX_LOADSTRING, _T("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\0"),
                    displayTiming[0],
                    displayTiming[HFRONTPORCH],
                    displayTiming[2],
                    displayTiming[3],
                    displayTiming[4],
                    displayTiming[VFRONTPORCH],
                    displayTiming[6],
                    displayTiming[7],
                    displayTiming[PIXELCLOCK],
                    displayTiming[9]
                   );

    // Lower than nominal update frequency
    StringCchPrintf(slower, MAX_LOADSTRING, _T("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\0"),
                    displayTiming[0],
                    displayTiming[HFRONTPORCH] + columnDelta,
                    displayTiming[2],
                    displayTiming[3],
                    displayTiming[4],
                    displayTiming[VFRONTPORCH] + lineDelta,
                    displayTiming[6],
                    displayTiming[7],
                    displayTiming[PIXELCLOCK],
                    displayTiming[9]
                   );

    totalColumns = displayTiming[HACTIVE] + displayTiming[HFRONTPORCH] + displayTiming[HSYNCWIDTH] + displayTiming[HBACKPORCH];
    totalLines = displayTiming[VACTIVE] + displayTiming[VFRONTPORCH] + displayTiming[VSYNCWIDTH] + displayTiming[VBACKPORCH];
    pixelClock = 1000 * displayTiming[PIXELCLOCK]; // Pixels/s
    displayFreqCruise = (double)pixelClock / (totalLines * totalColumns); // Frames/s
    displayFreqSlower = (double)pixelClock / ((totalLines + lineDelta) * (totalColumns + columnDelta));
    displayFreqFaster = (double)pixelClock / ((totalLines - lineDelta) * (totalColumns - columnDelta));
    curDisplayFreq = displayFreqCruise;
    GlobalDeleteAtom(getTiming);
    adjDelta = 0;
    powerstripTimingExists = TRUE;
    return S_OK;
}

// Reset display timing parameters to nominal.
HRESULT CGenlock::ResetTiming()
{
    LPARAM lParam = 0;
    WPARAM wParam = monitor;
    ATOM setTiming;
    LRESULT ret;
    CAutoLock lock(&csGenlockLock);

    if (!PowerstripRunning()) {
        return E_FAIL;
    }

    if (displayAdjustmentsMade > 0) {
        setTiming = GlobalAddAtom(cruise);
        lParam = setTiming;
        ret = SendMessage(psWnd, UM_SETCUSTOMTIMINGFAST, wParam, lParam);
        GlobalDeleteAtom(setTiming);
        curDisplayFreq = displayFreqCruise;
    }
    adjDelta = 0;
    return S_OK;
}

// Reset reference clock speed to nominal.
HRESULT CGenlock::ResetClock()
{
    adjDelta = 0;
    if (syncClock == nullptr) {
        return E_FAIL;
    } else {
        return syncClock->AdjustClock(1.0);
    }
}

HRESULT CGenlock::SetTargetSyncOffset(double targetD)
{
    targetSyncOffset = targetD;
    lowSyncOffset = targetD - controlLimit;
    highSyncOffset = targetD + controlLimit;
    return S_OK;
}

HRESULT CGenlock::GetTargetSyncOffset(double* targetD)
{
    *targetD = targetSyncOffset;
    return S_OK;
}

HRESULT CGenlock::SetControlLimit(double cL)
{
    controlLimit = cL;
    return S_OK;
}

HRESULT CGenlock::GetControlLimit(double* cL)
{
    *cL = controlLimit;
    return S_OK;
}

HRESULT CGenlock::SetDisplayResolution(UINT columns, UINT lines)
{
    visibleColumns = columns;
    visibleLines = lines;
    return S_OK;
}

__declspec(nothrow noalias) void CGenlock::AdviseSyncClock(__inout ISyncClock* sC)
{
    if (syncClock) {
        syncClock = nullptr;    // Release any outstanding references if this is called repeatedly
    }
    syncClock = sC;
}

// Set the monitor to control. This is best done manually as not all monitors can be controlled
// so automatic detection of monitor to control might have unintended effects.
// The PowerStrip API uses zero-based monitor numbers, i.e. the default monitor is 0.
HRESULT CGenlock::SetMonitor(UINT mon)
{
    monitor = mon;
    return S_OK;
}

HRESULT CGenlock::ResetStats()
{
    CAutoLock lock(&csGenlockLock);
    minSyncOffset = 1000000.0;
    maxSyncOffset = -1000000.0;
    minFrameCycle = 1000000.0;
    maxFrameCycle = -1000000.0;
    displayAdjustmentsMade = 0;
    clockAdjustmentsMade = 0;
    return S_OK;
}

// Synchronize by adjusting display refresh rate
HRESULT CGenlock::ControlDisplay(double syncOffset, double frameCycle)
{
    LPARAM lParam = 0;
    WPARAM wParam = monitor;
    ATOM setTiming;

    targetSyncOffset = mk_pRendererSettings->fTargetSyncOffset;
    lowSyncOffset = targetSyncOffset - mk_pRendererSettings->fControlLimit;
    highSyncOffset = targetSyncOffset + mk_pRendererSettings->fControlLimit;

    syncOffsetAvg = syncOffsetFifo->Average(syncOffset);
    minSyncOffset = min(minSyncOffset, syncOffset);
    maxSyncOffset = max(maxSyncOffset, syncOffset);
    frameCycleAvg = frameCycleFifo->Average(frameCycle);
    minFrameCycle = min(minFrameCycle, frameCycle);
    maxFrameCycle = max(maxFrameCycle, frameCycle);

    if (!PowerstripRunning() || !powerstripTimingExists) {
        return E_FAIL;
    }
    // Adjust as seldom as possible by checking the current controlState before changing it.
    if ((syncOffsetAvg > highSyncOffset) && (adjDelta != 1))
        // Speed up display refresh rate by subtracting pixels from the image.
    {
        adjDelta = 1; // Increase refresh rate
        curDisplayFreq = displayFreqFaster;
        setTiming = GlobalAddAtom(faster);
        lParam = setTiming;
        SendMessage(psWnd, UM_SETCUSTOMTIMINGFAST, wParam, lParam);
        GlobalDeleteAtom(setTiming);
        displayAdjustmentsMade++;
    } else
        // Slow down display refresh rate by adding pixels to the image.
        if ((syncOffsetAvg < lowSyncOffset) && (adjDelta != -1)) {
            adjDelta = -1;
            curDisplayFreq = displayFreqSlower;
            setTiming = GlobalAddAtom(slower);
            lParam = setTiming;
            SendMessage(psWnd, UM_SETCUSTOMTIMINGFAST, wParam, lParam);
            GlobalDeleteAtom(setTiming);
            displayAdjustmentsMade++;
        } else
            // Cruise.
            if ((syncOffsetAvg < targetSyncOffset) && (adjDelta == 1)) {
                adjDelta = 0;
                curDisplayFreq = displayFreqCruise;
                setTiming = GlobalAddAtom(cruise);
                lParam = setTiming;
                SendMessage(psWnd, UM_SETCUSTOMTIMINGFAST, wParam, lParam);
                GlobalDeleteAtom(setTiming);
                displayAdjustmentsMade++;
            } else if ((syncOffsetAvg > targetSyncOffset) && (adjDelta == -1)) {
                adjDelta = 0;
                curDisplayFreq = displayFreqCruise;
                setTiming = GlobalAddAtom(cruise);
                lParam = setTiming;
                SendMessage(psWnd, UM_SETCUSTOMTIMINGFAST, wParam, lParam);
                GlobalDeleteAtom(setTiming);
                displayAdjustmentsMade++;
            }
    return S_OK;
}

// Synchronize by adjusting reference clock rate (and therefore video FPS).
// Todo: check so that we don't have a live source
HRESULT CGenlock::ControlClock(double syncOffset, double frameCycle)
{
    targetSyncOffset = mk_pRendererSettings->fTargetSyncOffset;
    lowSyncOffset = targetSyncOffset - mk_pRendererSettings->fControlLimit;
    highSyncOffset = targetSyncOffset + mk_pRendererSettings->fControlLimit;

    syncOffsetAvg = syncOffsetFifo->Average(syncOffset);
    minSyncOffset = min(minSyncOffset, syncOffset);
    maxSyncOffset = max(maxSyncOffset, syncOffset);
    frameCycleAvg = frameCycleFifo->Average(frameCycle);
    minFrameCycle = min(minFrameCycle, frameCycle);
    maxFrameCycle = max(maxFrameCycle, frameCycle);

    if (!syncClock) {
        return E_FAIL;
    }
    // Adjust as seldom as possible by checking the current controlState before changing it.
    if ((syncOffsetAvg > highSyncOffset) && (adjDelta != 1))
        // Slow down video stream.
    {
        adjDelta = 1;
        syncClock->AdjustClock(1.0 - cycleDelta); // Makes the clock move slower by providing smaller increments
        clockAdjustmentsMade++;
    } else
        // Speed up video stream.
        if ((syncOffsetAvg < lowSyncOffset) && (adjDelta != -1)) {
            adjDelta = -1;
            syncClock->AdjustClock(1.0 + cycleDelta);
            clockAdjustmentsMade++;
        } else
            // Cruise.
            if ((syncOffsetAvg < targetSyncOffset) && (adjDelta == 1)) {
                adjDelta = 0;
                syncClock->AdjustClock(1.0);
                clockAdjustmentsMade++;
            } else if ((syncOffsetAvg > targetSyncOffset) && (adjDelta == -1)) {
                adjDelta = 0;
                syncClock->AdjustClock(1.0);
                clockAdjustmentsMade++;
            }
    return S_OK;
}

// Don't adjust anything, just update the syncOffset stats
HRESULT CGenlock::UpdateStats(double syncOffset, double frameCycle)
{
    syncOffsetAvg = syncOffsetFifo->Average(syncOffset);
    minSyncOffset = min(minSyncOffset, syncOffset);
    maxSyncOffset = max(maxSyncOffset, syncOffset);
    frameCycleAvg = frameCycleFifo->Average(frameCycle);
    minFrameCycle = min(minFrameCycle, frameCycle);
    maxFrameCycle = max(maxFrameCycle, frameCycle);
    return S_OK;
}