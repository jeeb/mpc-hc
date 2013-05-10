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
#include "InternalDX9Shaders.h"
#include "../../../thirdparty/lcms2/library/include/lcms2.h"
#include "EVRAllocatorPresenter.h"
#include "IPinHook.h"
#include <D3Dcompiler.h>
#include <io.h>
#include <FCNTL.H>
#include <intrin.h>
#include "../../../thirdparty/VirtualDub/h/vd2/system/cpuaccel.h"

#if INTENT_ABSOLUTE_COLORIMETRIC != 3 || INTENT_RELATIVE_COLORIMETRIC != 1
#error LCMS defines changed, edit this class and the renderer settings handlers to correct this issue
#endif

extern "C" __declspec(nothrow noalias) void __fastcall MoveDitherMatrix(void* pDst, float const* pQuantization);// Assembly function, can be removed once the compiler decently assembles the base C++ function
extern "C" __declspec(nothrow noalias) void __fastcall MoveDitherMatrixAVX(void* pDst, float const* pQuantization);// Assembly function, can be removed once the compiler decently assembles the base C++ function

using namespace DSObjects;

extern __declspec(nothrow noalias noinline noreturn) void DSObjects::ErrBox(__in HRESULT hr, __in wchar_t const* szText)// be careful not to use this function for recoverable errors, as it aborts the program
{
    ASSERT(szText);

    // error message box function
    if (hr) {// HRESULT will be converted to verbose for output
        MessageBoxW(nullptr, szText, GetWindowsErrorMessage(hr, nullptr), MB_SYSTEMMODAL | MB_ICONERROR);
        // note: the string object returned by GetWindowsErrorMessage() goes out of scope here
    } else {// the neutral "error" title caption will be used
        MessageBoxW(nullptr, szText, nullptr, MB_SYSTEMMODAL | MB_ICONERROR);
    }
    // warning: abort() does not clean up any dynamic objects on the stack and does not return, take all precautions to not leak memory when calling this function
    abort();
}// TODO: create a better handling function

extern __declspec(nothrow noalias noinline) unsigned __int8 DSObjects::GetChromaType(__in unsigned __int32 u32SubType)
{
    switch (u32SubType) {
        case FCC('P016'):
        case FCC('P010'):
        case FCC('NV12'):
        case FCC('I420'):
        case FCC('IYUV'):
        case FCC('YV12'):
            return 2;// 4:2:0
        case FCC('Y216'):
        case FCC('P216'):
        case FCC('v216'):
        case FCC('Y210'):
        case FCC('P210'):
        case FCC('v210'):
        case FCC('YUY2'):
        case FCC('YVYU'):
        case FCC('UYVY'):
        case FCC('Y42T'):
            return 1;// 4:2:2
    }
    return 0;// just assume 4:4:4
}

// CDX9AllocatorPresenter

__declspec(nothrow noalias) CDX9AllocatorPresenter::CDX9AllocatorPresenter(__in HWND hWnd, __inout CStringW* pstrError, __in bool bIsEVR)
    : CSubPicAllocatorPresenterImpl(hWnd)
    , CUtilityWindow(AfxGetApp()->m_pMainWnd->m_hWnd)// m_hCallbackWnd is commonly used for all callbacks to the main window
    , mk_bIsEVR(bIsEVR)
    , m_hCurrentMonitor(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST))
    , m_pRenderersData(GetRenderersData())
#ifdef D3D_DEBUG_INFO
    , m_hD3D9(LoadLibraryW(L"d3d9d.dll"))
    , m_hD3DX9Dll(LoadLibraryW(L"d3dx9d_43.dll"))
#else
    , m_hD3D9(LoadLibraryW(L"d3d9.dll"))
    , m_hD3DX9Dll(LoadLibraryW(L"d3dx9_43.dll"))
#endif
    , m_hDXVA2Lib(LoadLibraryW(L"dxva2.dll"))
    , m_bPartialExDeviceReset(false)
    , m_fPixelShaderCounter(0.0f)
    , mk_i64MustBeZero(0)
    , m_bDesktopCompositionDisabled(false)
    , m_boCompositionEnabled(FALSE)
    , m_dRoundedStreamReferenceVideoFrameRate(25.0)// set to 25 fps, like the subtitle renderer's default, this needs to be initialized to something for the first call to Paint()
{
    static_assert(!(offsetof(CDX9AllocatorPresenter, m_afStatsBarsJGraph) & 15), "structure alignment test failed, edit this class and CSubPicAllocatorPresenterImpl to correct the issue");
    static_assert(!(offsetof(CDX9AllocatorPresenter, m_dDetectedRefreshRate) & 15), "structure alignment test failed, edit this class and CSubPicAllocatorPresenterImpl to correct the issue");
    ASSERT(pstrError);

    // zero-initialize items, this needs to be placed before any return statements for proper operation of the class destructor, see header file for reference
    __m128 xZero = _mm_setzero_ps();// only a command to set a register to zero, this should not add constant value to the assembly
    static_assert(!(offsetof(CDX9AllocatorPresenter, m_dReferenceRefreshRate) & 15), "structure alignment test failed, edit this class to correct the issue");
    static_assert(!((offsetof(CDX9AllocatorPresenter, m_pSubtitleTexture) + sizeof(m_pSubtitleTexture) - offsetof(CDX9AllocatorPresenter, m_dReferenceRefreshRate)) & 15), "modulo 16 byte count for routine data set test failed, edit this class to correct the issue");
    unsigned __int32 u32Erase = static_cast<unsigned __int32>((offsetof(CDX9AllocatorPresenter, m_pSubtitleTexture) + sizeof(m_pSubtitleTexture) - offsetof(CDX9AllocatorPresenter, m_dReferenceRefreshRate)) >> 4);
    float* pDst = reinterpret_cast<float*>(&m_dReferenceRefreshRate);
    do {
        _mm_stream_ps(pDst, xZero);// zero-fills target
        pDst += 4;
    } while (--u32Erase);// 16 aligned bytes are written every time

    if (!m_hD3D9) {
        *pstrError = L"Could not find d3d9.dll\n";
        ASSERT(0);
        return;
    }

#if D3DX_SDK_VERSION != 43
#error DirectX SDK June 2010 (v43) is required to build this, if the DirectX SDK has been updated, add loading functions to this part of the code and the class initializer
#endif// this code has duplicates in ShaderEditorDlg.cpp, DX9AllocatorPresenter.cpp and SyncRenderer.cpp
    // load latest compatible version of the DLL that is available
    if (m_hD3DX9Dll) {
        m_hD3DCompiler = LoadLibraryW(L"D3DCompiler_43.dll");
    } else {
        ASSERT(0);
        *pstrError = L"The installed DirectX End-User Runtime is outdated. Please download and install the June 2010 release or newer in order for MPC-HC to function properly.\n";
        return;
    }

    {
        // import functions from d3dx9_43.dll
        uintptr_t pModule = reinterpret_cast<uintptr_t>(m_hD3DX9Dll);// just a named alias
        IMAGE_DOS_HEADER const* pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER const*>(pModule);
        IMAGE_NT_HEADERS const* pNTHeader = reinterpret_cast<IMAGE_NT_HEADERS const*>(pModule + static_cast<size_t>(static_cast<ULONG>(pDOSHeader->e_lfanew)));
        IMAGE_EXPORT_DIRECTORY const* pEAT = reinterpret_cast<IMAGE_EXPORT_DIRECTORY const*>(pModule + static_cast<size_t>(pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress));
        uintptr_t pAONbase = pModule + static_cast<size_t>(pEAT->AddressOfNames);
        uintptr_t pAONObase = pModule + static_cast<size_t>(pEAT->AddressOfNameOrdinals);
        uintptr_t pAOFbase = pModule + static_cast<size_t>(pEAT->AddressOfFunctions);
        DWORD dwLoopCount = pEAT->NumberOfNames - 1;
        {
            __declspec(align(8)) static char const kszFunc[] = "D3DXLoadSurfaceFromSurface";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
            ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
            for (;;) {
                unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)
                        && *reinterpret_cast<__int16 __unaligned const*>(kszName + 24) == *reinterpret_cast<__int16 const*>(kszFunc + 24)
                        && kszName[26] == kszFunc[26]) {// note that this part must compare zero end inclusive
                    // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                    break;
                } else if (--i < 0) {
                    ASSERT(0);
                    goto d3dx9_43EHandling;
                }
            }
            unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
            unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
            m_fnD3DXLoadSurfaceFromSurface = reinterpret_cast<D3DXLoadSurfaceFromSurfacePtr>(pModule + static_cast<size_t>(u32AOF));
        }
        {
            __declspec(align(8)) static char const kszFunc[] = "D3DXGetPixelShaderProfile";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
            ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
            for (;;) {
                unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)
                        && *reinterpret_cast<__int16 __unaligned const*>(kszName + 24) == *reinterpret_cast<__int16 const*>(kszFunc + 24)) {// note that this part must compare zero end inclusive
                    // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                    break;
                } else if (--i < 0) {
                    ASSERT(0);
                    goto d3dx9_43EHandling;
                }
            }
            unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
            unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
            m_fnD3DXGetPixelShaderProfile = reinterpret_cast<D3DXGetPixelShaderProfilePtr>(pModule + static_cast<size_t>(u32AOF));
        }
        {
            __declspec(align(8)) static char const kszFunc[] = "D3DXCreateLine";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
            ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
            for (;;) {
                unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                        && *reinterpret_cast<__int32 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int32 const*>(kszFunc + 8)
                        && *reinterpret_cast<__int16 __unaligned const*>(kszName + 12) == *reinterpret_cast<__int16 const*>(kszFunc + 12)
                        && kszName[14] == kszFunc[14]) {// note that this part must compare zero end inclusive
                    // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                    break;
                } else if (--i < 0) {
                    ASSERT(0);
                    goto d3dx9_43EHandling;
                }
            }
            unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
            unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
            m_fnD3DXCreateLine = reinterpret_cast<D3DXCreateLinePtr>(pModule + static_cast<size_t>(u32AOF));
        }
        {
            __declspec(align(8)) static char const kszFunc[] = "D3DXCreateFontW";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
            ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
            for (;;) {
                unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)) {// note that this part must compare zero end inclusive
                    // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                    break;
                } else if (--i < 0) {
                    ASSERT(0);
                    goto d3dx9_43EHandling;
                }
            }
            unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
            unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
            m_fnD3DXCreateFontW = reinterpret_cast<D3DXCreateFontWPtr>(pModule + static_cast<size_t>(u32AOF));
        }
        goto Skipd3dx9_43EHandling;
d3dx9_43EHandling:
        *pstrError = L"Could not read data from d3dx9_43.dll.\n";
        return;
    }
Skipd3dx9_43EHandling:

    {
        // import function from D3DCompiler_43.dll
        uintptr_t pModule = reinterpret_cast<uintptr_t>(m_hD3DCompiler);// just a named alias
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
                *pstrError = L"Could not read data from D3DCompiler_43.dll.\n";
                return;
            }
        }
        unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pModule + static_cast<size_t>(pEAT->AddressOfNameOrdinals) + i * 2);// table of two-byte elements
        unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pModule + static_cast<size_t>(pEAT->AddressOfFunctions) + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
        m_fnD3DCompile = reinterpret_cast<D3DCompilePtr>(pModule + static_cast<size_t>(u32AOF));
    }

    {
        // store the OS version
        OSVERSIONINFOW version;
        version.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
        EXECUTE_ASSERT(GetVersionExW(&version));
        m_u8OSVersionMajor = static_cast<unsigned __int8>(version.dwMajorVersion);
        m_u8OSVersionMinor = static_cast<unsigned __int8>(version.dwMinorVersion);
        m_u16OSVersionBuild = static_cast<unsigned __int16>(version.dwBuildNumber);
    }

    if (m_u8OSVersionMajor >= 6) {// Vista and higher
#ifndef DISABLE_USING_D3D9EX
        {
            // import function from d3d9.dll
            uintptr_t pModule = reinterpret_cast<uintptr_t>(m_hD3D9);// just an named alias
            IMAGE_DOS_HEADER const* pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER const*>(pModule);
            IMAGE_NT_HEADERS const* pNTHeader = reinterpret_cast<IMAGE_NT_HEADERS const*>(pModule + static_cast<size_t>(static_cast<ULONG>(pDOSHeader->e_lfanew)));
            IMAGE_EXPORT_DIRECTORY const* pEAT = reinterpret_cast<IMAGE_EXPORT_DIRECTORY const*>(pModule + static_cast<size_t>(pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress));
            uintptr_t pAONbase = pModule + static_cast<size_t>(pEAT->AddressOfNames);

            __declspec(align(8)) static char const kszFunc[] = "Direct3DCreate9Ex";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
            ptrdiff_t i = static_cast<size_t>(pEAT->NumberOfNames - 1);// convert to signed for the loop system and pointer-sized for the pointer operations
            for (;;) {
                unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                        && *reinterpret_cast<__int16 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int16 const*>(kszFunc + 16)) {// note that this part must compare zero end inclusive
                    // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                    break;
                } else if (--i < 0) {
                    ASSERT(0);
                    *pstrError = L"Could not read data from d3d9.dll.\n";
                    return;
                }
            }
            unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pModule + static_cast<size_t>(pEAT->AddressOfNameOrdinals) + i * 2);// table of two-byte elements
            unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pModule + static_cast<size_t>(pEAT->AddressOfFunctions) + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
            typedef HRESULT(WINAPI * Direct3DCreate9ExPtr)(__in UINT SDKVersion, __out IDirect3D9Ex** ppD3D);
            Direct3DCreate9ExPtr fnDirect3DCreate9Ex = reinterpret_cast<Direct3DCreate9ExPtr>(pModule + static_cast<size_t>(u32AOF));

            HRESULT hr = fnDirect3DCreate9Ex(D3D_SDK_VERSION, &m_pD3D);// see the header for IDirect3D9 and IDirect3D9Ex modes for m_pD3D
            if (FAILED(hr)) {
                ASSERT(0);
                *pstrError = L"Failed to create D3D9Ex\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                return;
            }
        }
#endif
        // register the window class
        ASSERT(!m_u16RegisteredWindowClassAtom);// m_u16RegisteredWindowClassAtom should be 0 at this point
        m_u16RegisteredWindowClassAtom = RegisterClassW(&gk_wUtilityWindowClassDef);
        if (!m_u16RegisteredWindowClassAtom) {
            ASSERT(0);
            *pstrError = L"Failed to register the window class\n";
            return;
        }

        {
            // import functions from dxva2.dll
            uintptr_t pModule = reinterpret_cast<uintptr_t>(m_hDXVA2Lib);// just a named alias
            if (!pModule) {
                *pstrError = L"Could not find dxva2.dll\n";
                ASSERT(0);
                return;
            }
            IMAGE_DOS_HEADER const* pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER const*>(pModule);
            IMAGE_NT_HEADERS const* pNTHeader = reinterpret_cast<IMAGE_NT_HEADERS const*>(pModule + static_cast<size_t>(static_cast<ULONG>(pDOSHeader->e_lfanew)));
            IMAGE_EXPORT_DIRECTORY const* pEAT = reinterpret_cast<IMAGE_EXPORT_DIRECTORY const*>(pModule + static_cast<size_t>(pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress));
            uintptr_t pAONbase = pModule + static_cast<size_t>(pEAT->AddressOfNames);
            uintptr_t pAONObase = pModule + static_cast<size_t>(pEAT->AddressOfNameOrdinals);
            uintptr_t pAOFbase = pModule + static_cast<size_t>(pEAT->AddressOfFunctions);
            DWORD dwLoopCount = pEAT->NumberOfNames - 1;
            {
                __declspec(align(8)) static char const kszFunc[] = "GetNumberOfPhysicalMonitorsFromHMONITOR";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
                ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
                for (;;) {
                    unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                    char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                    if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 24) == *reinterpret_cast<__int64 const*>(kszFunc + 24)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 32) == *reinterpret_cast<__int64 const*>(kszFunc + 32)) {// note that this part must compare zero end inclusive
                        // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                        break;
                    } else if (--i < 0) {
                        ASSERT(0);
                        goto dxva2EHandling;
                    }
                }
                unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                m_fnGetNumberOfPhysicalMonitorsFromHMONITOR = reinterpret_cast<GetNumberOfPhysicalMonitorsFromHMONITORPtr>(pModule + static_cast<size_t>(u32AOF));
            }
            {
                __declspec(align(8)) static char const kszFunc[] = "GetPhysicalMonitorsFromHMONITOR";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
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
                        goto dxva2EHandling;
                    }
                }
                unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                m_fnGetPhysicalMonitorsFromHMONITOR = reinterpret_cast<GetPhysicalMonitorsFromHMONITORPtr>(pModule + static_cast<size_t>(u32AOF));
            }
            {
                __declspec(align(8)) static char const kszFunc[] = "DestroyPhysicalMonitors";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
                ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
                for (;;) {
                    unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                    char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                    if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)) {// note that this part must compare zero end inclusive
                        // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                        break;
                    } else if (--i < 0) {
                        ASSERT(0);
                        goto dxva2EHandling;
                    }
                }
                unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                m_fnDestroyPhysicalMonitors = reinterpret_cast<DestroyPhysicalMonitorsPtr>(pModule + static_cast<size_t>(u32AOF));
            }
            {
                __declspec(align(8)) static char const kszFunc[] = "GetTimingReport";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
                ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
                for (;;) {
                    unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                    char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                    if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)) {// note that this part must compare zero end inclusive
                        // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                        break;
                    } else if (--i < 0) {
                        ASSERT(0);
                        goto dxva2EHandling;
                    }
                }
                unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                m_fnGetTimingReport = reinterpret_cast<GetTimingReportPtr>(pModule + static_cast<size_t>(u32AOF));
            }
            if (mk_bIsEVR) {// the DXVA2 device manager is only used by EVR
                __declspec(align(8)) static char const kszFunc[] = "DXVA2CreateDirect3DDeviceManager9";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
                ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
                for (;;) {
                    unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                    char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                    if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 24) == *reinterpret_cast<__int64 const*>(kszFunc + 24)
                            && *reinterpret_cast<__int16 __unaligned const*>(kszName + 32) == *reinterpret_cast<__int16 const*>(kszFunc + 32)) {// note that this part must compare zero end inclusive
                        // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                        break;
                    } else if (--i < 0) {
                        ASSERT(0);
                        goto dxva2EHandling;
                    }
                }
                unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                m_fnDXVA2CreateDirect3DDeviceManager9 = reinterpret_cast<DXVA2CreateDirect3DDeviceManager9Ptr>(pModule + static_cast<size_t>(u32AOF));
            }
            goto Skipdxva2EHandling;
dxva2EHandling:
            *pstrError = L"Could not read data from dxva2.dll.\n";
            return;
        }
Skipdxva2EHandling:

        {
            // import functions from dwmapi.dll
            HINSTANCE hDWMAPI = LoadLibraryW(L"dwmapi.dll");
            if (!hDWMAPI) {
                *pstrError = L"Could not find dwmapi.dll\n";
                ASSERT(0);
                return;
            }
            m_hDWMAPI = hDWMAPI;
            uintptr_t pModule = reinterpret_cast<uintptr_t>(hDWMAPI);// just a named alias
            IMAGE_DOS_HEADER const* pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER const*>(pModule);
            IMAGE_NT_HEADERS const* pNTHeader = reinterpret_cast<IMAGE_NT_HEADERS const*>(pModule + static_cast<size_t>(static_cast<ULONG>(pDOSHeader->e_lfanew)));
            IMAGE_EXPORT_DIRECTORY const* pEAT = reinterpret_cast<IMAGE_EXPORT_DIRECTORY const*>(pModule + static_cast<size_t>(pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress));
            uintptr_t pAONbase = pModule + static_cast<size_t>(pEAT->AddressOfNames);
            uintptr_t pAONObase = pModule + static_cast<size_t>(pEAT->AddressOfNameOrdinals);
            uintptr_t pAOFbase = pModule + static_cast<size_t>(pEAT->AddressOfFunctions);
            DWORD dwLoopCount = pEAT->NumberOfNames - 1;
            {
                __declspec(align(8)) static char const kszFunc[] = "DwmEnableMMCSS";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
                ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
                for (;;) {
                    unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                    char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                    if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                            && *reinterpret_cast<__int32 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int32 const*>(kszFunc + 8)
                            && *reinterpret_cast<__int16 __unaligned const*>(kszName + 12) == *reinterpret_cast<__int16 const*>(kszFunc + 12)
                            && kszName[14] == kszFunc[14]) {// note that this part must compare zero end inclusive
                        // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                        break;
                    } else if (--i < 0) {
                        ASSERT(0);
                        goto dwmapiEHandling;
                    }
                }
                unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                m_fnDwmEnableMMCSS = reinterpret_cast<DwmEnableMMCSSPtr>(pModule + static_cast<size_t>(u32AOF));
            }
            {
                __declspec(align(8)) static char const kszFunc[] = "DwmSetDxFrameDuration";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
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
                        goto dwmapiEHandling;
                    }
                }
                unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                m_fnDwmSetDxFrameDuration = reinterpret_cast<DwmSetDxFrameDurationPtr>(pModule + static_cast<size_t>(u32AOF));
            }
            {
                __declspec(align(8)) static char const kszFunc[] = "DwmSetPresentParameters";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
                ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
                for (;;) {
                    unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                    char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                    if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)) {// note that this part must compare zero end inclusive
                        // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                        break;
                    } else if (--i < 0) {
                        ASSERT(0);
                        goto dwmapiEHandling;
                    }
                }
                unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                m_fnDwmSetPresentParameters = reinterpret_cast<DwmSetPresentParametersPtr>(pModule + static_cast<size_t>(u32AOF));
            }
            {
                __declspec(align(8)) static char const kszFunc[] = "DwmGetCompositionTimingInfo";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
                ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
                for (;;) {
                    unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                    char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                    if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)
                            && *reinterpret_cast<__int32 __unaligned const*>(kszName + 24) == *reinterpret_cast<__int32 const*>(kszFunc + 24)) {// note that this part must compare zero end inclusive
                        // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                        break;
                    } else if (--i < 0) {
                        ASSERT(0);
                        goto dwmapiEHandling;
                    }
                }
                unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                m_fnDwmGetCompositionTimingInfo = reinterpret_cast<DwmGetCompositionTimingInfoPtr>(pModule + static_cast<size_t>(u32AOF));
            }
            {
                __declspec(align(8)) static char const kszFunc[] = "DwmIsCompositionEnabled";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
                ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
                for (;;) {
                    unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                    char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                    if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)) {// note that this part must compare zero end inclusive
                        // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                        break;
                    } else if (--i < 0) {
                        ASSERT(0);
                        goto dwmapiEHandling;
                    }
                }
                unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                m_fnDwmIsCompositionEnabled = reinterpret_cast<DwmIsCompositionEnabledPtr>(pModule + static_cast<size_t>(u32AOF));
            }
            {
                __declspec(align(8)) static char const kszFunc[] = "DwmEnableComposition";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
                ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
                for (;;) {
                    unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                    char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                    if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                            && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                            && *reinterpret_cast<__int32 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int32 const*>(kszFunc + 16)
                            && kszName[20] == kszFunc[20]) {// note that this part must compare zero end inclusive
                        // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                        break;
                    } else if (--i < 0) {
                        ASSERT(0);
                        goto dwmapiEHandling;
                    }
                }
                unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                m_fnDwmEnableComposition = reinterpret_cast<DwmEnableCompositionPtr>(pModule + static_cast<size_t>(u32AOF));
            }
            goto SkipdwmapiEHandling;
dwmapiEHandling:
            *pstrError = L"Could not read data from dwmapi.dll.\n";
            return;
        }
SkipdwmapiEHandling:

        // handle desktop composition
        if (m_u16OSVersionMinorMajor >= 0x602) {// Windows 8 doesn't allow disabling desktop composition
            m_boCompositionEnabled = TRUE;
        } else if (mk_pRendererSettings->iVMRDisableDesktopComposition) {
            m_bDesktopCompositionDisabled = true;
            m_fnDwmEnableComposition(DWM_EC_DISABLECOMPOSITION);
        } else {
            m_fnDwmIsCompositionEnabled(&m_boCompositionEnabled);
        }

#ifdef DISABLE_USING_D3D9EX
        goto NoExModeUseDirect3DCreate9;
#endif
    } else {
#ifdef DISABLE_USING_D3D9EX
NoExModeUseDirect3DCreate9:
#endif
        // import function from d3d9.dll
        uintptr_t pModule = reinterpret_cast<uintptr_t>(m_hD3D9);// just an named alias
        IMAGE_DOS_HEADER const* pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER const*>(pModule);
        IMAGE_NT_HEADERS const* pNTHeader = reinterpret_cast<IMAGE_NT_HEADERS const*>(pModule + static_cast<size_t>(static_cast<ULONG>(pDOSHeader->e_lfanew)));
        IMAGE_EXPORT_DIRECTORY const* pEAT = reinterpret_cast<IMAGE_EXPORT_DIRECTORY const*>(pModule + static_cast<size_t>(pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress));
        uintptr_t pAONbase = pModule + static_cast<size_t>(pEAT->AddressOfNames);

        __declspec(align(8)) static char const kszFunc[] = "Direct3DCreate9";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
        ptrdiff_t i = static_cast<size_t>(pEAT->NumberOfNames - 1);// convert to signed for the loop system and pointer-sized for the pointer operations
        for (;;) {
            unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
            char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
            if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                    && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)) {// note that this part must compare zero end inclusive
                // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                break;
            } else if (--i < 0) {
                ASSERT(0);
                *pstrError = L"Could not read data from d3d9.dll.\n";
                return;
            }
        }
        unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pModule + static_cast<size_t>(pEAT->AddressOfNameOrdinals) + i * 2);// table of two-byte elements
        unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pModule + static_cast<size_t>(pEAT->AddressOfFunctions) + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
        typedef IDirect3D9* (WINAPI * Direct3DCreate9Ptr)(__in UINT SDKVersion);
        Direct3DCreate9Ptr fnDirect3DCreate9 = reinterpret_cast<Direct3DCreate9Ptr>(pModule + static_cast<size_t>(u32AOF));

        IDirect3D9* pD3D = fnDirect3DCreate9(D3D_SDK_VERSION);
        if (!m_pD3D) {
            ASSERT(0);
            *pstrError = L"Failed to create D3D9\n";
            return;
        }
        m_pD3D = static_cast<IDirect3D9Ex*>(pD3D);// see the header for IDirect3D9 and IDirect3D9Ex modes for m_pD3D
    }

    // select the adapter
    __declspec(align(16)) D3DADAPTER_IDENTIFIER9 adapterIdentifier;
    if (UINT uiAdapterCount = m_pD3D->GetAdapterCount()) {
        static_assert(sizeof(adapterIdentifier.Description) == 512, "D3DADAPTER_IDENTIFIER9 component size changed");
        INT i = uiAdapterCount - 1;// convert to signed for the loop system
        // find the selected adapter, note that the GUIDVRendererDevice will be nullptr if the box is unchecked
        if (i && (mk_pRendererSettings->GUIDVRendererDevice[0] || mk_pRendererSettings->GUIDVRendererDevice[1])) {
            do {
                if (SUCCEEDED(m_pD3D->GetAdapterIdentifier(i, 0, &adapterIdentifier))
                        && (reinterpret_cast<__int64*>(&adapterIdentifier.DeviceIdentifier)[0] == mk_pRendererSettings->GUIDVRendererDevice[0])
                        && (reinterpret_cast<__int64*>(&adapterIdentifier.DeviceIdentifier)[1] == mk_pRendererSettings->GUIDVRendererDevice[1])) {
                    m_uiCurrentAdapter = static_cast<UINT>(i);
                    goto RegularContinue;
                }
            } while (--i >= 0);
            ASSERT(0);
            *pstrError = L"Failed to find the selected render device in the 'Options', 'Output', 'Playback' tab\n";
            return;
        }

        do {// use the adapter associated with the monitor
            HMONITOR hAdpMon = m_pD3D->GetAdapterMonitor(i);
            if (hAdpMon == m_hCurrentMonitor) {
                if (SUCCEEDED(m_pD3D->GetAdapterIdentifier(i, 0, &adapterIdentifier))) {
                    m_uiCurrentAdapter = i;
                    goto RegularContinue;
                }
            }
        } while (--i >= 0);
        ASSERT(0);
        *pstrError = L"Failed to select a suitable render device associated with the current monitor, another compatible render device may be present on the current system\n";
        return;
    }
    ASSERT(0);
    *pstrError = L"Failed to find any suitable render device\n";
    return;
RegularContinue:// an efficient break from both loops
    // copy adapter description
    size_t j = 0;
    char sc;
    do {
        sc = adapterIdentifier.Description[j];
        if (!sc) {
            break;
        }
        m_awcD3D9Device[j] = static_cast<wchar_t>(sc);
    } while (++j < MAX_DEVICE_IDENTIFIER_STRING);
    m_upLenstrD3D9Device = j;

    static_assert(sizeof(adapterIdentifier.DeviceName) == 32, "D3DADAPTER_IDENTIFIER9 component size changed");
    __m128 lowername = _mm_load_ps(reinterpret_cast<float*>(adapterIdentifier.DeviceName));
    __m128 uppername = _mm_load_ps(reinterpret_cast<float*>(adapterIdentifier.DeviceName) + 4);
    _mm_store_ps(reinterpret_cast<float*>(m_szGDIDisplayDeviceName), lowername);
    _mm_store_ps(reinterpret_cast<float*>(m_szGDIDisplayDeviceName) + 4, uppername);
    m_pRenderersData->m_dwPCIVendor = adapterIdentifier.VendorId;

    HRESULT hr;
    // get the device caps
    EXECUTE_ASSERT(S_OK == (hr = m_pD3D->GetDeviceCaps(m_uiCurrentAdapter, D3DDEVTYPE_HAL, &m_dcCaps)));// m_uiCurrentAdapter is validated before this function, the others are nothing special, this function should never fail

    // partially initialize the base shader macro for level
    // keep this list the same as in InternalDX9Shaders.cpp
    // taken care of after device creation
    m_aShaderMacros[0].Name = "Ml";// PS 3.0 level compile: 0 or 1
    m_aShaderMacros[1].Name = "Mr";// surface color intervals: 0 for [0, 1], 1 for [16384/65535, 49151/65535]
    m_aShaderMacros[2].Name = "Mq";// maximum quantized integer value of the display color format, 255 is for 8-bit, 1023 is for 10-bit, function: pow(2, [display bits per component])-1
    // partially initialized in initializer, taken care of after device creation and in the legacy swapchain resize routine
    m_aShaderMacros[3].Name = "Mw";// pre-resize width
    m_aShaderMacros[4].Name = "Mh";// pre-resize height
    m_aShaderMacros[5].Name = "Ma";// post-resize width
    m_aShaderMacros[6].Name = "Mv";// post-resize height
    // partially initialized in initializer, taken care of in the resizer routine (the resizer pixel shaders should be the first to initialize in the renderer)
    m_aShaderMacros[7].Name = "Mb";// resize output width (not limited by the post-resize width)
    m_aShaderMacros[8].Name = "Mu";// resize output height (not limited by the post-resize height)
    m_aShaderMacros[9].Name = "Me";// resizer pass context-specific width (as the parameter can be either pre- or post-resize width for two-pass resizers)
    // initialized in initializer, taken care of in the final pass routine
    m_aShaderMacros[10].Name = "Mm";// XYZ to RGB matrix
    m_aShaderMacros[11].Name = "Mc";// color management: disabled == 0, Little CMS == 1, Limited Color Ranges == 2
    m_aShaderMacros[12].Name = "Ms";// LUT3D samples in each U, V and W dimension
    m_aShaderMacros[13].Name = "Md";// dithering levels: no dithering == 0, static ordered dithering == 1, random ordered dithering == 2, adaptive random dithering >= 3
    m_aShaderMacros[14].Name = "Mt";// dithering test: 0 or 1
    // initialized in initializer, set by the mixer, taken care of in the final pass routine
    m_aShaderMacros[15].Name = "My";// Y'CbCr chroma cositing: 0 for horizontal chroma cositing, 1 for no horizontal chroma cositing
    // initialized in initializer, taken care of in the frame interpolation routine
    m_aShaderMacros[16].Name = "Mf";// area factor for adaptive frame interpolation

    m_aShaderMacros[3].Definition = m_szVideoWidth;
    m_aShaderMacros[4].Definition = m_szVideoHeight;
    m_aShaderMacros[5].Definition = m_szWindowWidth;
    m_aShaderMacros[6].Definition = m_szWindowHeight;
    m_aShaderMacros[7].Definition = m_szResizedWidth;
    m_aShaderMacros[8].Definition = m_szResizedHeight;
    m_aShaderMacros[9].Definition = m_szWindowWidth;
    m_aShaderMacros[10].Definition = m_szXYZtoRGBmatrix;
    m_aShaderMacros[11].Definition = m_szColorManagementLevel;
    m_aShaderMacros[12].Definition = m_szLut3DSize;
    m_aShaderMacros[13].Definition = m_szDitheringLevel;
    m_aShaderMacros[14].Definition = m_szDitheringTest;
    m_aShaderMacros[15].Definition = m_szChromaCositing;
    m_aShaderMacros[16].Definition = m_szFrameInterpolationLevel;
    m_aShaderMacros[17].Name = nullptr;
    m_aShaderMacros[17].Definition = nullptr;

    // initialize strings partially
    *reinterpret_cast<__int16*>(m_szXYZtoRGBmatrix) = *reinterpret_cast<__int16*>(m_szColorManagementLevel) = *reinterpret_cast<__int16*>(m_szLut3DSize) = *reinterpret_cast<__int16*>(m_szDitheringLevel) = *reinterpret_cast<__int16*>(m_szDitheringTest) = *reinterpret_cast<__int16*>(m_szChromaCositing) = *reinterpret_cast<__int16*>(m_szFrameInterpolationLevel) = static_cast<unsigned __int16>('0');// also sets a zero end, for some it clears the last character for the entire lifetime of this class
    *reinterpret_cast<__int64*>(m_szVideoWidth) = *reinterpret_cast<__int64*>(m_szVideoHeight) = *reinterpret_cast<__int64*>(m_szWindowWidth) = *reinterpret_cast<__int64*>(m_szResizedWidth) = *reinterpret_cast<__int64*>(m_szWindowHeight) = *reinterpret_cast<__int64*>(m_szResizedHeight) = static_cast<unsigned __int64>('x0');// the upper bytes are set to zero, the last characters are cleared for the entire lifetime of this class
    m_szMonitorName[13] = 0;// clear the last character for the entire lifetime of this class, to make sure that the string is zero-ended; the longest name the EDID supports is 13 characters

    // set up the timer data
    EXECUTE_ASSERT(QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&m_u64PerfFreq)));
    m_dPerfFreqr = 1.0 / static_cast<double>(static_cast<__int64>(m_u64PerfFreq));// the standard converter only does a proper job with signed values

    // partially initialize the present parameters struct
    m_dpPParam.MultiSampleType = D3DMULTISAMPLE_NONE;
    m_dpPParam.MultiSampleQuality = 0;
    m_dpPParam.EnableAutoDepthStencil = FALSE;
    m_dpPParam.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
    m_dpPParam.Flags = D3DPRESENTFLAG_VIDEO;

    // retrieves the monitor EDID info
    ReadDisplay();

    // set the base offset for all internal timer calculations
    EXECUTE_ASSERT(QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&m_i64PerfCntInit)));

    CreateDevice(pstrError);
}

__declspec(nothrow noalias) CDX9AllocatorPresenter::~CDX9AllocatorPresenter()
{
    if (m_bDesktopCompositionDisabled) {
        m_fnDwmEnableComposition(DWM_EC_ENABLECOMPOSITION);
    }
    if (m_hEvtQuitVSync) {
        SetEvent(m_hEvtQuitVSync);
        if (m_hVSyncThread) {
            if (WaitForSingleObject(m_hVSyncThread, 10000) == WAIT_TIMEOUT) {
                ASSERT(0);
                TerminateThread(m_hVSyncThread, 0xDEAD);
            }
            CloseHandle(m_hVSyncThread);
            m_hVSyncThread = nullptr;
        }
        CloseHandle(m_hEvtQuitVSync);
        m_hEvtQuitVSync = nullptr;
    }
    ULONG u;
    // note that the class destructor is also invoked on many critical failures, so all class resources may or may not be available in this function

    // final releases to destroy the device
    if (m_pD3DDev) {
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

        // release standard resources
        // IUnknown is the most primary interface for all of these interface pointers, none of the pointers are "fat" (none of the function classes will be made with multiple virtual inheritance)
        ptrdiff_t i = (offsetof(CDX9AllocatorPresenter, m_pSubtitleTexture) + sizeof(m_pSubtitleTexture) - offsetof(CDX9AllocatorPresenter, m_pLine)) / sizeof(uintptr_t) - 1;
        do {
            IUnknown* pu = reinterpret_cast<IUnknown**>(&m_pLine)[i];
            if (pu) {
                pu->Release();
            }
        } while (--i >= 0);

        POSITION pos = m_apCustomPixelShaders[0].GetHeadPosition();
        while (pos) {
            EXTERNALSHADER& ceShader = m_apCustomPixelShaders[0].GetNext(pos);
            if (ceShader.pSrcData) {
                u = ceShader.pSrcData->Release();
                ASSERT(!u);
                if (ceShader.pPixelShader) {
                    u = ceShader.pPixelShader->Release();
                    ASSERT(!u);
                }
            } else {
                ASSERT(!ceShader.pPixelShader);
            }
            ASSERT(ceShader.strSrcData);
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
            _aligned_free(ceShader.strSrcData);
#else
            free(ceShader.strSrcData);
#endif
        }
        pos = m_apCustomPixelShaders[1].GetHeadPosition();
        while (pos) {
            EXTERNALSHADER& ceShader = m_apCustomPixelShaders[1].GetNext(pos);
            if (ceShader.pSrcData) {
                u = ceShader.pSrcData->Release();
                ASSERT(!u);
                if (ceShader.pPixelShader) {
                    u = ceShader.pPixelShader->Release();
                    ASSERT(!u);
                }
            } else {
                ASSERT(!ceShader.pPixelShader);
            }
            ASSERT(ceShader.strSrcData);
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
            _aligned_free(ceShader.strSrcData);
#else
            free(ceShader.strSrcData);
#endif
        }

        // GPU command queue flush
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
            u = m_pUtilEventQuery->Release();
            ASSERT(!u);
        }

        // todo: set DMode/DModeEx and the m_dpPParam refresh rate items to [UINT display refresh rate] if the mode should be changed on exit, Width and Height can be modified, too, this does need a reorganization of the m_uiBaseRefreshRate item
        if (m_dpPParam.BackBufferFormat == D3DFMT_A2R10G10B10) {// 10-bit mode needs a proper shutdown
            ASSERT(m_u8OSVersionMajor >= 6);// Vista and newer only
            m_dpPParam.BackBufferCount = 1;
            m_dpPParam.BackBufferFormat = D3DFMT_X8R8G8B8;
            m_dpPParam.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
            D3DDISPLAYMODEEX DModeEx = {sizeof(D3DDISPLAYMODEEX), m_dpPParam.BackBufferWidth, m_dpPParam.BackBufferHeight, m_uiBaseRefreshRate, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_PROGRESSIVE};
            m_pD3DDev->ResetEx(&m_dpPParam, &DModeEx);
        }
        // semantics for a reset to windowed mode with the mode changes mentioned in the comment above can be seen in ResetMainDevice()

        u = m_pD3DDev->Release();
        ASSERT(!u);
        goto ReleaseD3D;// pointer guaranteed to be valid
    }
    if (m_pD3D) {
ReleaseD3D:
        u = m_pD3D->Release();
        ASSERT(!u);
    }

    // free library references
    if (m_hDWMAPI) {
        EXECUTE_ASSERT(FreeLibrary(m_hDWMAPI));
    }
    if (m_hDXVA2Lib) {
        EXECUTE_ASSERT(FreeLibrary(m_hDXVA2Lib));
    }
    if (m_hD3DCompiler) {
        EXECUTE_ASSERT(FreeLibrary(m_hD3DCompiler));
    }
    if (m_hD3DX9Dll) {
        EXECUTE_ASSERT(FreeLibrary(m_hD3DX9Dll));
    }
    if (m_hD3D9) {
        EXECUTE_ASSERT(FreeLibrary(m_hD3D9));
    }

    if (m_u16RegisteredWindowClassAtom) {// remove extra window class registration
        if (m_hUtilityWnd) {
            EXECUTE_ASSERT(DestroyWindow(m_hUtilityWnd));
        }
        EXECUTE_ASSERT(UnregisterClassW(reinterpret_cast<wchar_t const*>(static_cast<uintptr_t>(m_u16RegisteredWindowClassAtom)), nullptr));
    }
}

#define FVBLANK_TAKEND3DLOCK 1
#define FVBLANK_WAITIFINSIDE 2
#define FVBLANK_WAITED 4
#define FVBLANK_MEASURESTATS 8

__declspec(nothrow noalias) __forceinline void CDX9AllocatorPresenter::VSyncThread()
{
    DWORD dwObject;
    do {
        // Do our stuff
        // FIXME: this wastes CPU cycles when the renderer is paused, and we don't want that. Only enter here if the renderer is in running state.

        if (m_pD3DDev) {
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
            __m128d xMH = _mm_load1_pd(&m_dMonitorHeight);
            __m128d x0 = _mm_mul_sd(xMH, *reinterpret_cast<__m128d*>(&m_dReferenceRefreshRate));// should do mulsd xmm m64
            static __declspec(align(16)) double const adP1[2] = {0.003, 1.0 / 3.0};
            __m128d xa = _mm_mul_pd(x0, *reinterpret_cast<__m128d const*>(adP1));// output is: low (a) 1.8 ms, high (b) 33% of Time
            __m128d xb = _mm_shuffle_pd(xa, xa, _MM_SHUFFLE2(1, 1));// move high to low
            __m128d xc = _mm_min_sd(xa, xb);
            __m128d x2 = _mm_set_sd(5.0);
            __m128d x3 = _mm_max_sd(xc, x2);
            ASSERT(_MM_GET_ROUNDING_MODE() == _MM_ROUND_NEAREST);
            __int32 MinRange = _mm_cvtsd_si32(x3);// rounding cast value to __int32
#else
            double a = 0.003 * m_dMonitorHeight * m_dReferenceRefreshRate;// 1.8 ms
            double b = m_dMonitorHeight * (1.0 / 3.0);// 33% of Time
            double c = (a < b) ? a : b;
            __int32 MinRange = (c > 5.0) ? static_cast<__int32>(c + 0.5) : 5;// 1.8 ms or max 33% of Time
#endif
            __int32 VSyncPos = mk_pRendererSettings->iVMR9VSyncOffset;
            __int32 ScreenHeightDiv = m_i32MonitorHeight / 40;
            __int32 WaitRange = (ScreenHeightDiv > 5) ? ScreenHeightDiv : 5;
            VSyncPos += MinRange + WaitRange;
            if (VSyncPos < 0) {
                VSyncPos += m_i32MonitorHeight;
            } else if (VSyncPos > m_i32MonitorHeight) {
                VSyncPos -= m_i32MonitorHeight;
            }

            __int32 ScanLine = VSyncPos + 1;
            if (ScanLine > m_i32MonitorHeight) {
                ScanLine -= m_i32MonitorHeight;
            }

            __int32 ScanLineMiddle = ScanLine + (static_cast<unsigned __int32>(m_i32MonitorHeight) >> 1);
            if (ScanLineMiddle < 0) {
                ScanLineMiddle += m_i32MonitorHeight;
            } else if (ScanLineMiddle > m_i32MonitorHeight) {
                ScanLineMiddle -= m_i32MonitorHeight;
            }

            __int32 ScanlineStart = ScanLine;
            LARGE_INTEGER liVBlankRange;// out: unsigned __int8 VBlank flags low, __int32 scanline number at end high
            liVBlankRange.QuadPart = WaitForVBlankRange(ScanlineStart, 5, FVBLANK_WAITIFINSIDE);
            unsigned __int8 u8VBFlags = static_cast<unsigned __int8>(liVBlankRange.LowPart);// save the flags
            ScanlineStart = liVBlankRange.HighPart;
            LARGE_INTEGER liTimeStart, liScanLineMiddle;
            EXECUTE_ASSERT(QueryPerformanceCounter(&liTimeStart));

            liVBlankRange.QuadPart = WaitForVBlankRange(ScanLineMiddle, 5, u8VBFlags);
            u8VBFlags = static_cast<unsigned __int8>(liVBlankRange.LowPart);// save the flags
            ScanLineMiddle = liVBlankRange.HighPart;
            EXECUTE_ASSERT(QueryPerformanceCounter(&liScanLineMiddle));

            __int32 ScanlineEnd = ScanLine;
            liVBlankRange.QuadPart = WaitForVBlankRange(ScanlineEnd, 5, u8VBFlags);
            ScanlineEnd = liVBlankRange.HighPart;
            EXECUTE_ASSERT(QueryPerformanceCounter(&m_liLastPerfCnt));

            double nSeconds = static_cast<double>(m_liLastPerfCnt.QuadPart - liTimeStart.QuadPart) * m_dPerfFreqr;
            double dDiffStartToMiddle = static_cast<double>(liScanLineMiddle.QuadPart - liTimeStart.QuadPart) * m_dPerfFreqr;
            double dDiffMiddleToEnd = static_cast<double>(m_liLastPerfCnt.QuadPart - liScanLineMiddle.QuadPart) * m_dPerfFreqr;
            double DiffDiff = (dDiffMiddleToEnd > dDiffStartToMiddle) ? dDiffMiddleToEnd / dDiffStartToMiddle : dDiffStartToMiddle / dDiffMiddleToEnd;

            if (nSeconds > 0.003 && DiffDiff < 1.3) {
                double ScanLineSeconds;
                double nScanLines;
                if (ScanLineMiddle > ScanlineEnd) {
                    ScanLineSeconds = dDiffStartToMiddle;
                    nScanLines = static_cast<double>(ScanLineMiddle - ScanlineStart);
                } else {
                    ScanLineSeconds = dDiffMiddleToEnd;
                    nScanLines = static_cast<double>(ScanlineEnd - ScanLineMiddle);
                }

                double ScanLineTime = ScanLineSeconds / nScanLines;

                size_t upPos = m_upDetectedRefreshRatePos & 127;// modulo action by low bitmask
                ++m_upDetectedRefreshRatePos;// this only increments, so it will wrap around, on a 32-bit integer it's once a year with 120 fps video
                m_adDetectedScanlineRateList[upPos] = ScanLineTime;
                if (m_dDetectedScanlineTime && ScanlineStart != ScanlineEnd) {
                    __int32 Diff = ScanlineEnd - ScanlineStart;
                    nSeconds -= static_cast<double>(Diff) * m_dDetectedScanlineTime;
                }
                m_adDetectedRefreshRateList[upPos] = nSeconds;
                double Average = 0.0;
                double AverageScanline = 0.0;
                unsigned __int8 u8Pos = (upPos < 128) ? static_cast<unsigned __int8>(upPos) : 128;
                ptrdiff_t i = u8Pos - 1;
                if (i >= 0) {
                    do {
                        Average += m_adDetectedRefreshRateList[i];
                        AverageScanline += m_adDetectedScanlineRateList[i];
                    } while (--i >= 0);

                    double dPosr = 1.0 / static_cast<double>(u8Pos);
                    Average *= dPosr;
                    AverageScanline *= dPosr;
                } else {
                    Average = 0.0;
                    AverageScanline = 0.0;
                }

                if (Average > 0.0 && AverageScanline > 0.0) {
                    CAutoLock Lock(&m_csRefreshRateLock);
                    if (m_dDetectedRefreshTime / Average > 1.01 || m_dDetectedRefreshTime / Average < 0.99) {
                        m_dDetectedRefreshTime = Average;
                        m_dDetectedRefreshTimePrim = 0.0;
                    }

                    // moderate the values
                    m_dDetectedRefreshTimePrim = -0.375 * (m_dDetectedRefreshTime - Average);
                    m_dDetectedRefreshTime += m_dDetectedRefreshTimePrim;

                    if (m_dDetectedRefreshTime > 0.0) {
                        m_dDetectedRefreshRate = 1.0 / m_dDetectedRefreshTime;
                    } else {
                        m_dDetectedRefreshRate = m_dReferenceRefreshRate;
                    }

                    if (!m_dDetectedScanlineTime || m_dDetectedScanlineTime / AverageScanline > 1.01 || m_dDetectedScanlineTime / AverageScanline < 0.99) {
                        m_dDetectedScanlineTime = AverageScanline;
                        m_dDetectedScanlineTimePrim = 0.0;
                    }

                    // moderate the values
                    m_dDetectedScanlineTimePrim = -0.5 * m_dDetectedScanlineTimePrim + (m_dDetectedScanlineTime - AverageScanline) * -0.5625;
                    m_dDetectedScanlineTime += m_dDetectedScanlineTimePrim;

                    if (m_dDetectedScanlineTime > 0.0) {
                        m_dDetectedScanlinesPerFrame = m_dDetectedRefreshTime / m_dDetectedScanlineTime;
                    } else {
                        m_dDetectedScanlinesPerFrame = m_dMonitorHeight;
                    }
                }
                //TRACE(L"Video renderer current refresh rate: %f\n", RefreshRate);
            }
        } else {
            m_dDetectedRefreshRate = m_dReferenceRefreshRate;
            m_dDetectedScanlinesPerFrame = m_dMonitorHeight;
        }

        dwObject = WaitForSingleObject(m_hEvtQuitVSync, 1);
    } while (dwObject != WAIT_OBJECT_0);
}

__declspec(nothrow noalias) DWORD WINAPI CDX9AllocatorPresenter::VSyncThreadStatic(__in LPVOID lpParam)
{
    ASSERT(lpParam);

    DEBUG_ONLY(SetThreadName(0xFFFFFFFF, "CDX9AllocatorPresenter::VSyncThread"));
    reinterpret_cast<CDX9AllocatorPresenter*>(lpParam)->VSyncThread();
    return 0;
}

__declspec(nothrow noalias) void CDX9AllocatorPresenter::CreateDevice(CStringW* pstrError)
{
    ASSERT(pstrError);

    HRESULT hr;
    m_dpPParam.hDeviceWindow = m_hVideoWnd;// use the video window by default, also required to support D3D FS switching, note that the windowed mode on Vista and newer will overwrite this again with the custom-made window

    // non-zero initialize 6 values for VSync functions
    // 0x3FF00000 is big enough and convienient to use here
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
    _mm_stream_si32(&m_i32VBlankMin, 0x3FF00000);
    _mm_stream_si32(&m_i32VBlankEndPresent, 0x3FF00000);
    _mm_stream_si32(reinterpret_cast<__int32*>(&m_u32InitialVSyncWait), 0x3FF00000);// the value here doesn't matter at all, it's just convienient to re-use here
    // doubles to 1.0
    _mm_stream_si32(reinterpret_cast<__int32*>(&m_dPaintTimeMin), 0);
    _mm_stream_si32(reinterpret_cast<__int32*>(&m_dPaintTimeMin) + 1, 0x3FF00000);
    _mm_stream_si32(reinterpret_cast<__int32*>(&m_dRasterStatusWaitTimeMin), 0);
    _mm_stream_si32(reinterpret_cast<__int32*>(&m_dRasterStatusWaitTimeMin) + 1, 0x3FF00000);
    _mm_stream_si32(reinterpret_cast<__int32*>(&m_dModeratedTimeSpeed), 0);
    _mm_stream_si32(reinterpret_cast<__int32*>(&m_dModeratedTimeSpeed) + 1, 0x3FF00000);
    // 0 is correct for these two in the fullscreen mode, this is corrected in Paint() later on for the windowed mode
#ifdef _M_X64
    _mm_stream_si64x(reinterpret_cast<__int64*>(&m_fWindowOnMonitorPosLeft), 0);// also clears m_fWindowOnMonitorPosTop
#else
    _mm_stream_si32(reinterpret_cast<__int32*>(&m_fWindowOnMonitorPosLeft), 0);
    _mm_stream_si32(reinterpret_cast<__int32*>(&m_fWindowOnMonitorPosTop), 0);
#endif
#else
    m_i32VBlankMin = 0x3FF00000;
    m_i32VBlankEndPresent = 0x3FF00000;
    m_u32InitialVSyncWait = 0x3FF00000;// the value here doesn't matter at all, it's just convienient to re-use here
    // doubles to 1.0
    reinterpret_cast<__int32*>(&m_dPaintTimeMin)[0] = 0;
    reinterpret_cast<__int32*>(&m_dPaintTimeMin)[1] = 0x3FF00000;
    reinterpret_cast<__int32*>(&m_dRasterStatusWaitTimeMin)[0] = 0;
    reinterpret_cast<__int32*>(&m_dRasterStatusWaitTimeMin)[1] = 0x3FF00000;
    reinterpret_cast<__int32*>(&m_dModeratedTimeSpeed)[0] = 0;
    reinterpret_cast<__int32*>(&m_dModeratedTimeSpeed)[1] = 0x3FF00000;
    // 0 is correct for these two in the fullscreen mode, this is corrected in Paint() later on for the windowed mode
    *reinterpret_cast<__int32*>(&m_fWindowOnMonitorPosLeft) = 0;
    *reinterpret_cast<__int32*>(&m_fWindowOnMonitorPosTop) = 0;
#endif

    // zero initialize many values for VSync functions, see header file for reference
    __m128 xZero = _mm_setzero_ps();// only a command to set a register to zero, this should not add constant value to the assembly
    static_assert(!(offsetof(CDX9AllocatorPresenter, m_bSyncStatsAvailable) & 15), "structure alignment test failed, edit this class to correct the issue");
    static_assert(!((offsetof(CDX9AllocatorPresenter, m_dMaxQueueDepth) + sizeof(m_dMaxQueueDepth) - offsetof(CDX9AllocatorPresenter, m_bSyncStatsAvailable)) & 15), "modulo 16 byte count for routine data set test failed, edit this class to correct the issue");
    unsigned __int32 u32Erase = static_cast<unsigned __int32>((offsetof(CDX9AllocatorPresenter, m_dMaxQueueDepth) + sizeof(m_dMaxQueueDepth) - offsetof(CDX9AllocatorPresenter, m_bSyncStatsAvailable)) >> 4);
    float* pDst = reinterpret_cast<float*>(&m_bSyncStatsAvailable);
    do {
        _mm_stream_ps(pDst, xZero);// zero-fills target
        pDst += 4;
    } while (--u32Erase);// 16 aligned bytes are written every time

    if (!m_bPartialExDeviceReset) {// items that are only reset during complete resets, note: partial resets are available for Vista and newer only
        // detect surfaces support, note: m_bFP32Support is only a full 32-bit on the mixer output, and it's 16-bit normalized unsigned integer for the rest of the renderer pipeline (this format is only supported on cards that also support 16- and 32-bit floating-point surfaces)
        m_pRenderersData->m_bFP32Support = SUCCEEDED(m_pD3D->CheckDeviceFormat(m_uiCurrentAdapter, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING, D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16));
        m_pRenderersData->m_bFP16Support = SUCCEEDED(m_pD3D->CheckDeviceFormat(m_uiCurrentAdapter, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING, D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F));
        m_pRenderersData->m_bUI10Support = SUCCEEDED(m_pD3D->CheckDeviceFormat(m_uiCurrentAdapter, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING, D3DRTYPE_TEXTURE, D3DFMT_A2R10G10B10));

        // set surfaces quality and set the correlated surface color intervals string
        unsigned __int8 u8SurfaceQ = mk_pRendererSettings->iVMR9SurfacesQuality;
        m_u8VMR9SurfacesQuality = u8SurfaceQ;
        if ((u8SurfaceQ == 3) && m_pRenderersData->m_bFP32Support) {
            m_dfSurfaceType = D3DFMT_A16B16G16R16;
            m_aShaderMacros[1].Definition = "1";
        } else if ((u8SurfaceQ >= 2) && m_pRenderersData->m_bFP16Support) {
            m_dfSurfaceType = D3DFMT_A16B16G16R16F;
            m_aShaderMacros[1].Definition = "0";
        } else if (u8SurfaceQ && m_pRenderersData->m_bUI10Support) {
            m_dfSurfaceType = D3DFMT_A2R10G10B10;
            m_aShaderMacros[1].Definition = "1";
        } else {
            m_dfSurfaceType = D3DFMT_X8R8G8B8;
            m_aShaderMacros[1].Definition = "0";
        }
        m_bHighColorResolutionCurrent = mk_pRendererSettings->iVMR9HighColorResolution;
    }
    bool bAVSync = mk_pRendererSettings->fVMR9AlterativeVSync;
    m_bVMR9AlterativeVSyncCurrent = bAVSync;
    // Configure alternative VSync; for Vista and newer disable it, unless in windowed mode without desktop composition
    m_bAlternativeVSync = bAVSync && ((m_u8OSVersionMajor < 6) || (!mk_pRendererSettings->bD3DFullscreen && !m_boCompositionEnabled));

    // quantization string default
    m_aShaderMacros[2].Definition = "255";// can be changed to "1023"

    // swap effect default
    m_dpPParam.SwapEffect = D3DSWAPEFFECT_DISCARD;// can be changed to D3DSWAPEFFECT_FLIPEX

    // detect vertex support
    DWORD dwBehaviorFlags;
    if (!(m_dcCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) || (m_dcCaps.VertexShaderVersion < D3DVS_VERSION(2, 0))) {
        dwBehaviorFlags = D3DCREATE_NOWINDOWCHANGES | D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE;
    } else {
        dwBehaviorFlags =
#ifndef _DEBUG// a pure device doesn't output a lot of debug information, see the guide on the control panel applet of the DirectX SDK for reference
            (m_dcCaps.DevCaps & D3DDEVCAPS_PUREDEVICE) ? D3DCREATE_NOWINDOWCHANGES | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE :
#endif
            D3DCREATE_NOWINDOWCHANGES | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE;
    }

    if (m_u8OSVersionMajor >= 6) {// Vista and newer only
        D3DDISPLAYMODEEX DModeEx;
        DModeEx.Size = sizeof(D3DDISPLAYMODEEX);
        m_pD3D->GetAdapterDisplayModeEx(m_uiCurrentAdapter, &DModeEx, nullptr);
        m_uiBaseRefreshRate = DModeEx.RefreshRate;
        INT iMHeight = DModeEx.Height;
        m_i32MonitorHeight = iMHeight;
        m_dMonitorHeight = static_cast<double>(iMHeight);

        if (m_bAlternativeVSync) {
            m_dpPParam.BackBufferCount = 1;
            m_dpPParam.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        } else {
            m_dpPParam.BackBufferCount = 3;
            m_dpPParam.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        }
        if (mk_pRendererSettings->bD3DFullscreen) {
            if (m_hUtilityWnd) {
                EXECUTE_ASSERT(DestroyWindow(m_hUtilityWnd));
                m_hUtilityWnd = nullptr;
            }

            // ensure window foreground focus
            // to unlock SetForegroundWindow() we need to imitate pressing [Alt] key
            bool bPressed = false;
            if (!(::GetAsyncKeyState(VK_MENU) & 0x8000)) {
                bPressed = true;
                ::keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY | 0, 0);
            }
            ::SetForegroundWindow(m_hVideoWnd);// no ASSERT here
            if (bPressed) {
                ::keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
            }

            m_dpPParam.BackBufferWidth = DModeEx.Width;
            m_dpPParam.BackBufferHeight = DModeEx.Height;
            m_dpPParam.Windowed = FALSE;
            m_dpPParam.FullScreen_RefreshRateInHz = DModeEx.RefreshRate;// todo: set both these items to [UINT inherited display refresh rate] if the mode should be changed, Width and Height can be modified, too (EnumAdapterModesEx also accepts resolution number (UINT Mode) from the standard display modes list)

            if (mk_pRendererSettings->iVMR9HighColorResolution && (m_dfSurfaceType != D3DFMT_X8R8G8B8) // exclusive mode allows changing the display format
                    && SUCCEEDED(m_pD3D->CheckDeviceType(m_uiCurrentAdapter, D3DDEVTYPE_HAL, D3DFMT_A2R10G10B10, D3DFMT_A2R10G10B10, FALSE))) {
                DModeEx.Format = D3DFMT_A2R10G10B10;
                m_aShaderMacros[2].Definition = "1023";// quantization string
            }
        } else {
            if (m_boCompositionEnabled) {
                EXECUTE_ASSERT(S_OK == (hr = m_fnDwmEnableMMCSS(TRUE)));// this item is to enable MMCSS on DWM, it doesn't need to be reverted on closing
            }

            RECT rc;
            EXECUTE_ASSERT(GetWindowRect(m_hVideoWnd, &rc));
            LONG ww = rc.right - rc.left;
            if (!ww) {
                ww = 1;
            }
            LONG wh = rc.bottom - rc.top;
            if (!wh) {
                wh = 1;
            }
            m_dpPParam.BackBufferWidth = ww;
            m_dpPParam.BackBufferHeight = wh;

            m_dpPParam.Windowed = TRUE;
            m_dpPParam.FullScreen_RefreshRateInHz = 0;

            if (m_boCompositionEnabled) {
                if (m_u16OSVersionMinorMajor >= 0x601) {
                    m_dpPParam.SwapEffect = D3DSWAPEFFECT_FLIPEX;
                }// FLIPEX for windowed mode with Aero on Windows 7 and newer, DISCARD for other modes

                if (!m_hUtilityWnd) {
                    ASSERT(m_u16RegisteredWindowClassAtom);// created in the class constructor
                    if (!CreateUtilityWindow(m_u16RegisteredWindowClassAtom, ww, wh)) {
                        ASSERT(0);
                        *pstrError = L"CreateUtilityWindow() failed\n";
                        return;
                    }
                } else {
                    EXECUTE_ASSERT(SetWindowPos(m_hUtilityWnd, HWND_TOP, 0, 0, ww, wh, SWP_NOZORDER | SWP_NOREDRAW | SWP_NOCOPYBITS | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING | SWP_DEFERERASE));// a lot of flags to indicate to only use the cx and cy size parameters, and set top-left position to the usual 0 (resizing without setting the position is not recommended)
                }
                m_dpPParam.hDeviceWindow = m_hUtilityWnd;// override the standard window to this child window because of the strong binding
            } else if (m_hUtilityWnd) {
                EXECUTE_ASSERT(DestroyWindow(m_hUtilityWnd));
                m_hUtilityWnd = nullptr;
            }
        }

        m_dpPParam.BackBufferFormat = DModeEx.Format;// always keep display and backbuffer format the same

        D3DDISPLAYMODEEX* pODModeEx = &DModeEx;
        // zero out pODModeEx in case of windowed
        size_t upFSMask = static_cast<size_t>(m_dpPParam.Windowed) - 1;// all ones of all zeroes
        pODModeEx = reinterpret_cast<D3DDISPLAYMODEEX*>(reinterpret_cast<uintptr_t>(pODModeEx) & upFSMask);
        if (m_bPartialExDeviceReset) {// only a partial reset
            unsigned __int8 i = 8;
            do {
                hr = m_pD3DDev->ResetEx(&m_dpPParam, pODModeEx);
                if (SUCCEEDED(hr)) {
                    goto ExDeviceCreated;
                }
                if (hr == D3DERR_DEVICELOST) {
                    goto ExDeviceLost;
                }
                Sleep(100);// prevent rapid retries
            } while (--i);
            *pstrError = L"ResetEx() of device failed\n";
            *pstrError += GetWindowsErrorMessage(hr, nullptr);
            ASSERT(0);
            return;
        } else {// create a new device
            unsigned __int8 i = 8;
            dwBehaviorFlags |= D3DCREATE_ENABLE_PRESENTSTATS;
            do {
                hr = m_pD3D->CreateDeviceEx(m_uiCurrentAdapter, D3DDEVTYPE_HAL, m_hVideoWnd, dwBehaviorFlags, &m_dpPParam, pODModeEx, &m_pD3DDev);// see the header for IDirect3DDevice9 and IDirect3DDevice9Ex modes for m_pD3DDev
                if (SUCCEEDED(hr)) {
                    goto ExDeviceCreated;
                }
                if (hr == D3DERR_DEVICELOST) {
                    goto ExDeviceLost;
                }
                Sleep(100);// prevent rapid retries
            } while (--i);
            *pstrError = L"CreateDeviceEx() failed initialization\n";
            *pstrError += GetWindowsErrorMessage(hr, nullptr);
            ASSERT(0);
            return;
        }

ExDeviceLost:
        TRACE(L"Video renderer CreateDeviceEx() or ResetEx() resulted in D3DERR_DEVICELOST, trying to reset\n");
        do {
            Sleep(100);// prevent rapid retries
            TRACE(L"Video renderer CheckDeviceState() used prior to reset\n");
            hr = m_pD3DDev->CheckDeviceState(nullptr);
        } while (hr == D3DERR_DEVICELOST);
        if (FAILED(hr = m_pD3DDev->ResetEx(&m_dpPParam, pODModeEx))) {
            *pstrError = L"ResetEx() of device after status of device lost failed\n";
            *pstrError += GetWindowsErrorMessage(hr, nullptr);
            ASSERT(0);
            return;
        }

ExDeviceCreated:
        if (!m_dReferenceRefreshRate) {
            if (mk_pRendererSettings->dRefreshRateAdjust == 1.0) {// automatic refresh rate detection mode
                if ((m_pRenderersData->m_dwPCIVendor == PCIV_ATI) || (m_pRenderersData->m_dwPCIVendor == PCIV_AMD)) {
                    if (SpecificForAMD()) {
                        goto ExRefreshRateSet;
                    }
                } else {
                    if (GenericForExMode()) {
                        goto ExRefreshRateSet;
                    }
                }
            }
            // manually adapted refresh rate mode, or timing report failed
            static_assert(sizeof(m_dpPParam.Windowed) == sizeof(UINT), "struct D3DPRESENT_PARAMETERS or platform settings changed");
            UINT uiFSMask = m_dpPParam.Windowed - 1;// all ones or all zeroes
            INT rr = m_dpPParam.FullScreen_RefreshRateInHz & uiFSMask | DModeEx.RefreshRate & ~uiFSMask;
            double drr = static_cast<double>(rr);// the standard converter only does a proper job with signed values
            drr = mk_pRendererSettings->dRefreshRateAdjust * ((rr == 119 || rr == 89 || rr == 71 || rr == 59 || rr == 47 || rr == 29 || rr == 23) ? (drr + 1.0) * (1.0 / 1.001)/* NTSC adapted, don't include 95*/ : drr);// exact amounts
            m_dDetectedRefreshRate = m_dReferenceRefreshRate = drr;
            m_dDetectedRefreshTime = 1.0 / drr;
            m_dDetectedScanlinesPerFrame = m_dMonitorHeight;
        }
ExRefreshRateSet:
        UINT uiMaxLatency = 1;
        if (!m_bAlternativeVSync) {// set the maximum command latency time to 0.1875 s
            double dELT = m_dDetectedRefreshRate * 0.1875;
            if (dELT > 20.0) {// maximum allowed
                dELT = 20.0;
            }
            uiMaxLatency = static_cast<INT>(dELT);// the standard converter only does a proper job with signed values
        }
        m_pD3DDev->SetMaximumFrameLatency(uiMaxLatency);
        if (!m_bPartialExDeviceReset) {
            m_pD3DDev->SetGPUThreadPriority(7);// it's pretty much okay to put other rendering on hold when rendering video, the video renderer has many pauses in between batches, anyway
        }
    } else {
        D3DDISPLAYMODE DMode;
        m_pD3D->GetAdapterDisplayMode(m_uiCurrentAdapter, &DMode);
        m_uiBaseRefreshRate = DMode.RefreshRate;
        INT iMHeight = DMode.Height;
        m_i32MonitorHeight = iMHeight;
        m_dMonitorHeight = static_cast<double>(iMHeight);

        if (mk_pRendererSettings->bD3DFullscreen) {
            // ensure window foreground focus
            // to unlock SetForegroundWindow() we need to imitate pressing [Alt] key
            bool bPressed = false;
            if (!(::GetAsyncKeyState(VK_MENU) & 0x8000)) {
                bPressed = true;
                ::keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY | 0, 0);
            }
            ::SetForegroundWindow(m_hVideoWnd);// no ASSERT here
            if (bPressed) {
                ::keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
            }

            m_dpPParam.BackBufferWidth = DMode.Width;
            m_dpPParam.BackBufferHeight = DMode.Height;
            m_dpPParam.Windowed = FALSE;
            m_dpPParam.FullScreen_RefreshRateInHz = DMode.RefreshRate;// todo: set this item to [UINT inherited display refresh rate] if the mode should be changed, Width and Height can be modified, too (EnumAdapterModes also accepts resolution number (UINT Mode) from the standard display modes list)
            if (m_bAlternativeVSync) {
                m_dpPParam.BackBufferCount = 1;
                m_dpPParam.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
            } else {
                m_dpPParam.BackBufferCount = 3;
                m_dpPParam.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
            }
        } else {
            m_dpPParam.BackBufferWidth = 1;
            m_dpPParam.BackBufferHeight = 1;
            m_dpPParam.BackBufferCount = 1;
            m_dpPParam.Windowed = TRUE;
            m_dpPParam.FullScreen_RefreshRateInHz = 0;
            m_dpPParam.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        }

        m_dpPParam.BackBufferFormat = DMode.Format;// always keep display and backbuffer format the same

        ASSERT(!m_pD3DDev);// create a new device (partial resets on the IDirect3DDevice9Ex device are available for Vista and newer only)
        unsigned __int8 i = 8;
        do {
            hr = m_pD3D->CreateDevice(m_uiCurrentAdapter, D3DDEVTYPE_HAL, m_hVideoWnd, dwBehaviorFlags, &m_dpPParam, reinterpret_cast<IDirect3DDevice9**>(&m_pD3DDev));// see the header for IDirect3DDevice9 and IDirect3DDevice9Ex modes for m_pD3DDev
            if (SUCCEEDED(hr)) {
                goto DeviceCreated;
            }
            if (hr == D3DERR_DEVICELOST) {
                goto DeviceLost;
            }
            Sleep(100);// prevent rapid retries
        } while (--i);
        *pstrError = L"CreateDevice() failed initialization\n";
        *pstrError += GetWindowsErrorMessage(hr, nullptr);
        ASSERT(0);
        return;

DeviceLost:
        TRACE(L"Video renderer CreateDevice() resulted in D3DERR_DEVICELOST, trying to reset\n");
        do {
            Sleep(100);// prevent rapid retries
            TRACE(L"Video renderer TestCooperativeLevel used prior to reset\n");
            hr = m_pD3DDev->TestCooperativeLevel();
        } while (hr == D3DERR_DEVICELOST);
        if (FAILED(hr = m_pD3DDev->Reset(&m_dpPParam))) {
            *pstrError = L"Reset of device after status of device lost failed\n";
            *pstrError += GetWindowsErrorMessage(hr, nullptr);
            ASSERT(0);
            return;
        }

DeviceCreated:
        if (!m_dReferenceRefreshRate) {// manually adapted refresh rate mode, or timing report failed
            if (mk_pRendererSettings->dRefreshRateAdjust == 1.0) {// automatic refresh rate detection mode
                if ((m_pRenderersData->m_dwPCIVendor == PCIV_ATI) || (m_pRenderersData->m_dwPCIVendor == PCIV_AMD)) {
                    if (SpecificForAMD()) {
                        goto RefreshRateSet;
                    }
                }
            }
            static_assert(sizeof(m_dpPParam.Windowed) == sizeof(UINT), "struct D3DPRESENT_PARAMETERS or platform settings changed");
            UINT uiFSMask = m_dpPParam.Windowed - 1;// all ones or all zeroes
            INT rr = m_dpPParam.FullScreen_RefreshRateInHz & uiFSMask | DMode.RefreshRate & ~uiFSMask;
            double drr = static_cast<double>(rr);// the standard converter only does a proper job with signed values
            drr = mk_pRendererSettings->dRefreshRateAdjust * ((rr == 119 || rr == 89 || rr == 71 || rr == 59 || rr == 47 || rr == 29 || rr == 23) ? (drr + 1.0) * (1.0 / 1.001)/* NTSC adapted, don't include 95*/ : drr);// exact amounts
            m_dDetectedRefreshRate = m_dReferenceRefreshRate = drr;
            m_dDetectedRefreshTime = 1.0 / drr;
            m_dDetectedScanlinesPerFrame = m_dMonitorHeight;
        }
    }
RefreshRateSet:

    TRACE(L"Video renderer CreateDevice(): %s\n", GetWindowsErrorMessage(hr, m_hD3D9).GetBuffer());
    if (FAILED(hr)) {
        *pstrError = L"CreateDevice() failed\n";
        *pstrError += GetWindowsErrorMessage(hr, nullptr);
        ASSERT(0);
        return;
    }

    // initialize swap chains
    ASSERT(!m_pSwapChain);
    if ((m_u8OSVersionMajor >= 6) || !m_dpPParam.Windowed) {// Vista and newer, or fullscreen mode
        m_pD3DDev->GetSwapChain(0, reinterpret_cast<IDirect3DSwapChain9**>(&m_pSwapChain));// see the header for IDirect3DSwapChain9 and IDirect3DSwapChain9Ex modes for m_pSwapChain
    } else {
        RECT rc;
        EXECUTE_ASSERT(GetWindowRect(m_hVideoWnd, &rc));
        LONG ww = rc.right - rc.left;
        if (!ww) {
            ww = 1;
        }
        LONG wh = rc.bottom - rc.top;
        if (!wh) {
            wh = 1;
        }
        m_dpPParam.BackBufferWidth = ww;
        m_dpPParam.BackBufferHeight = wh;

        if (m_bAlternativeVSync) {
            m_dpPParam.BackBufferCount = 1;
            m_dpPParam.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        } else {
            m_dpPParam.BackBufferCount = 3;
            m_dpPParam.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        }
        m_pD3DDev->CreateAdditionalSwapChain(&m_dpPParam, reinterpret_cast<IDirect3DSwapChain9**>(&m_pSwapChain));// initialize the windowed swap chain, see the header for IDirect3DSwapChain9 and IDirect3DSwapChain9Ex modes for m_pSwapChain
    }

    // window size parts
    // note: m_u32WindowWidth and m_u32WindowHeight are configured by CSubPicAllocatorPresenterImpl, those are only used to check for resets
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
    __m128 x2 = _mm_set_ps1(1.0f);
    static_assert(sizeof(m_dpPParam.BackBufferWidth) == 4, "struct D3DPRESENT_PARAMETERS or platform settings changed");// the struct is declared on a global level
    __m128i xWS = _mm_loadl_epi64(reinterpret_cast<__m128i*>(&m_dpPParam.BackBufferWidth));
    __m128d x0 = _mm_cvtepi32_pd(xWS);// __int32 to double
    __m128 x1 = _mm_cvtpd_ps(x0);// double to float
    _mm_store_pd(&m_dWindowWidth, x0);// also stores m_dWindowHeight
    x2 = _mm_div_ps(x2, x1);// reciprocal trough _mm_rcp_ps() isn't accurate
    _mm_storel_pi(reinterpret_cast<__m64*>(&m_fWindowWidth), x1);// also stores m_fWindowHeight
    _mm_storel_pi(reinterpret_cast<__m64*>(&m_fWindowWidthr), x2);// also stores m_fWindowHeightr
#else
    m_dWindowWidth = static_cast<double>(static_cast<__int32>(m_dpPParam.BackBufferWidth));// the standard converter only does a proper job with signed values
    m_dWindowHeight = static_cast<double>(static_cast<__int32>(m_dpPParam.BackBufferHeight));
    m_fWindowWidth = static_cast<float>(m_dWindowWidth);
    m_fWindowHeight = static_cast<float>(m_dWindowHeight);
    m_fWindowWidthr = 1.0f / m_fWindowWidth;
    m_fWindowHeightr = 1.0f / m_fWindowHeight;
#endif
    unsigned __int8 u8Nibble;
    // m_szWindowWidth; standard method for converting numbers to hex strings
    ASSERT(m_dpPParam.BackBufferWidth <= 0x9FFFF);// the method implementation limit here
    u8Nibble = static_cast<unsigned __int8>(m_dpPParam.BackBufferWidth >> 16);// each hexadecimal char stores 4 bits
    m_szWindowWidth[2] = '0' + u8Nibble;
    u8Nibble = (m_dpPParam.BackBufferWidth >> 12) & 15;
    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
    m_szWindowWidth[3] = u8Nibble;
    u8Nibble = (m_dpPParam.BackBufferWidth >> 8) & 15;
    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
    m_szWindowWidth[4] = u8Nibble;
    u8Nibble = (m_dpPParam.BackBufferWidth >> 4) & 15;
    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
    m_szWindowWidth[5] = u8Nibble;
    u8Nibble = m_dpPParam.BackBufferWidth & 15;
    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
    m_szWindowWidth[6] = u8Nibble;

    // m_szWindowHeight; standard method for converting numbers to hex strings
    ASSERT(m_dpPParam.BackBufferHeight <= 0x9FFFF);// the method implementation limit here
    u8Nibble = static_cast<unsigned __int8>(m_dpPParam.BackBufferHeight >> 16);// each hexadecimal char stores 4 bits
    m_szWindowHeight[2] = '0' + u8Nibble;
    u8Nibble = (m_dpPParam.BackBufferHeight >> 12) & 15;
    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
    m_szWindowHeight[3] = u8Nibble;
    u8Nibble = (m_dpPParam.BackBufferHeight >> 8) & 15;
    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
    m_szWindowHeight[4] = u8Nibble;
    u8Nibble = (m_dpPParam.BackBufferHeight >> 4) & 15;
    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
    m_szWindowHeight[5] = u8Nibble;
    u8Nibble = m_dpPParam.BackBufferHeight & 15;
    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
    m_szWindowHeight[6] = u8Nibble;

    if (!m_bPartialExDeviceReset) {// items that are only reset during complete resets, note: partial resets are available for Vista and newer only
        // create an index buffer
        static __declspec(align(16)) __int16 const indices[6] = {0, 1, 2, 2, 1, 3};// two triangles
        ASSERT(!m_pIndexBuffer);
        hr = m_pD3DDev->CreateIndexBuffer(16, D3DUSAGE_DONOTCLIP | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &m_pIndexBuffer, nullptr);// the top 4 bytes are ignored
        void* pVoid;
        hr = m_pIndexBuffer->Lock(0, 0, &pVoid, 0);
        _mm_stream_ps(reinterpret_cast<float*>(pVoid), _mm_load_ps(reinterpret_cast<float const*>(indices)));
        hr = m_pIndexBuffer->Unlock();
        // set the static index buffer
        hr = m_pD3DDev->SetIndices(m_pIndexBuffer);

        m_pProfile = m_fnD3DXGetPixelShaderProfile(m_pD3DDev);// get the pixel shader profile level for the pixel shader compiler
        m_aShaderMacros[0].Definition = (m_dcCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0)) ? "1" : "0";

        // initialize device sampler states
        // I've never seen SetRenderState() or SetSamplerState() return anything but S_OK for valid states, so a simple EXECUTE_ASSERT on S_OK test is fine here.
        // some default sampler states are perfectly okay
        //EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(?, D3DSAMP_MAGFILTER, D3DTEXF_POINT)));
        //EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(?, D3DSAMP_MINFILTER, D3DTEXF_POINT)));
        //EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(?, D3DSAMP_MIPFILTER, D3DTEXF_NONE)));
        //EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(?, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP)));
        //EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(?, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP)));
        //EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE))); no Z-buffer is enabled, so this is default
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_CLIPPING, FALSE)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_LIGHTING, FALSE)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE)));
        // The current subtitle renderer uses pre-multiplied alpha (multiplication by 1.0-alpha on source, for D3DBLEND_ONE), this is very bad for blending colors. Change this characteristic of the subtitle renderer ASAP. D3DRS_DESTBLEND is fine as it is.
        // for the quality mode, the subtitle blending shader de-pre-multiplies the alpha by pixel shading and also does color conversion
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_SRCBLEND, (m_dfSurfaceType == D3DFMT_X8R8G8B8) ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCALPHA)));// inverse alpha channel for dst
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_ALPHAREF, 0xFF)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_LESS)));
        // enable these two when alpha blending, and disable them afterwards
        //EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE)));
        //EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE)));
        // general sampler stage options
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP)));
        // m_pD3DDev->SetTexture(1, m_pDitherTexture) is the only valid option for sampler 1, because of the non-power of 2 texture support rules for D3DTADDRESS_WRAP state samplers (D3DPTEXTURECAPS_NONPOW2CONDITIONAL, D3DPTEXTURECAPS_POW2)
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR)));// only used for the finalpass Lut3D volume texture
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(2, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP)));
        // only used in constant frame interpolation
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(3, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(3, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(4, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(4, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(5, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(5, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP)));
        // only used in the adaptive constant frame interpolation, for mipmapped vector textures
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(6, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(6, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(6, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(6, D3DSAMP_MINFILTER, D3DTEXF_LINEAR)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(7, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP)));// sampler 7 does need point sampling
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(7, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(8, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(8, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(8, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(8, D3DSAMP_MINFILTER, D3DTEXF_LINEAR)));

        // create subtitle renderer resources
        ASSERT(!m_pSubPicAllocator && !m_pSubPicQueue);// the reset sequence should always destroy the subtitle renderer resources
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
            u32sx = m_dpPParam.BackBufferWidth;
            u32sy = m_dpPParam.BackBufferHeight;
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

        void* pRawMem = malloc(sizeof(CDX9SubPicAllocator));
        if (!pRawMem) {
            *pstrError = L"Out of memory error for creating CDX9SubPicAllocator\n";
            ASSERT(0);
            return;
        }
        CDX9SubPicAllocator* pSubPicAllocator = new(pRawMem) CDX9SubPicAllocator(u32sx, u32sy, m_pD3DDev);
        m_pSubPicAllocator = static_cast<CSubPicAllocatorImpl*>(pSubPicAllocator);// reference inherited

        CSubPicQueueImpl* pSubPicQueue;
        if (mk_pRendererSettings->nSPCSize) {
            pRawMem = malloc(sizeof(CSubPicQueue));
            if (!pRawMem) {
                *pstrError = L"Out of memory error for creating CSubPicQueue\n";
                ASSERT(0);
                return;
            }
            pSubPicQueue = static_cast<CSubPicQueueImpl*>(new(pRawMem) CSubPicQueue(m_pSubPicAllocator, m_dDetectedVideoFrameRate, mk_pRendererSettings->nSPCSize, !mk_pRendererSettings->fSPCAllowAnimationWhenBuffering));
        } else {
            pRawMem = malloc(sizeof(CSubPicQueueNoThread));
            if (!pRawMem) {
                *pstrError = L"Out of memory error for creating CSubPicQueueNoThread\n";
                ASSERT(0);
                return;
            }
            pSubPicQueue = static_cast<CSubPicQueueImpl*>(new(pRawMem) CSubPicQueueNoThread(m_pSubPicAllocator, m_dDetectedVideoFrameRate));
        }
        m_pSubPicQueue = pSubPicQueue;// reference inherited
    } else {// just overwrite the device pointer for m_pSubPicAllocator
        static_cast<CDX9SubPicAllocator*>(m_pSubPicAllocator)->SetDevice(m_pD3DDev);
    }
    // note: partial resets always put the subtitle queue offline in ResetMainDevice() (and in turn, that correctly cleans out all internal SubPics from m_pSubPicAllocator and m_pSubPicQueue), so m_pSubPicProvider has to be set here again
    if (m_pSubPicProvider) {
        m_pSubPicQueue->SetSubPicProvider(m_pSubPicProvider);
    }

    // external pixel shaders; compile the base data if required, create the base pixel shader interface pointers
    POSITION pos = m_apCustomPixelShaders[0].GetHeadPosition();
    while (pos) {
        EXTERNALSHADER& ceShader = m_apCustomPixelShaders[0].GetNext(pos);
        if (!ceShader.pPixelShader) {
            if (!ceShader.pSrcData) {
                if (FAILED(hr = m_fnD3DCompile(ceShader.strSrcData, ceShader.u32SrcLen, nullptr, m_aShaderMacros, nullptr, "main", m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &ceShader.pSrcData, nullptr))) {
                    *pstrError = L"D3DCompile() failed\n";
                    *pstrError += GetWindowsErrorMessage(hr, nullptr);
                    ASSERT(0);
                }
            }
            if (FAILED(hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(ceShader.pSrcData->GetBufferPointer()), &ceShader.pPixelShader))) {
                *pstrError = L"CreatePixelShader() failed\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                ASSERT(0);
            }
        }
    }
    pos = m_apCustomPixelShaders[1].GetHeadPosition();
    while (pos) {
        EXTERNALSHADER& ceShader = m_apCustomPixelShaders[1].GetNext(pos);
        if (!ceShader.pPixelShader) {
            if (!ceShader.pSrcData) {
                if (FAILED(hr = m_fnD3DCompile(ceShader.strSrcData, ceShader.u32SrcLen, nullptr, m_aShaderMacros, nullptr, "main", m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &ceShader.pSrcData, nullptr))) {
                    *pstrError = L"D3DCompile() failed\n";
                    *pstrError += GetWindowsErrorMessage(hr, nullptr);
                    ASSERT(0);
                }
            }
            if (FAILED(hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(ceShader.pSrcData->GetBufferPointer()), &ceShader.pPixelShader))) {
                *pstrError = L"CreatePixelShader() failed\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                ASSERT(0);
            }
        }
    }

    ASSERT(!m_hVSyncThread);// the reset sequence should always close the thread
    if (m_bAlternativeVSync && mk_bIsEVR) {
        m_hEvtQuitVSync = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!m_hEvtQuitVSync) {// Don't create a thread with no stop switch
            *pstrError = L"CreateEvent() for the VSync thread failed\n";
            ASSERT(0);
            return;
        }
        m_hVSyncThread = ::CreateThread(nullptr, 0x10000, VSyncThreadStatic, this, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
        if (!m_hVSyncThread) {
            *pstrError = L"Creating the VSync thread failed\n";
            ASSERT(0);
            return;
        }
        EXECUTE_ASSERT(SetThreadPriority(m_hVSyncThread, THREAD_PRIORITY_HIGHEST));// the program should have proper access rights at this point for any priority except realtime
    }

    EXECUTE_ASSERT(QueryPerformanceCounter(&m_liLastPerfCnt));
    m_dPrevStartPaint = static_cast<double>(m_liLastPerfCnt.QuadPart - m_i64PerfCntInit) * m_dPerfFreqr;

    // get the back buffer, this will enable the render thread to render if it's currently on hold for a reset
    ASSERT(m_pSwapChain);
    ASSERT(!m_pBackBuffer);
    m_pSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &m_pBackBuffer);
    m_pD3DDev->ColorFill(m_pBackBuffer, nullptr, 0);// this is just to make sure that no backbuffers are ever presented uninitialized
}

__declspec(nothrow noalias noinline) void CDX9AllocatorPresenter::AllocSurfaces()
{
    // allocation of all video-sized surfaces and textures for the video mixer to use
    D3DFORMAT dfMixerSurfaceType = m_dfSurfaceType;
    // the mixer can't do a proper job on the 16-bit normalized unsigned integer surfaces unfortunately (color space conversion, expanded intervals), so we use A32B32G32R32F instead
    if (dfMixerSurfaceType == D3DFMT_A16B16G16R16) {
        dfMixerSurfaceType = D3DFMT_A32B32G32R32F;
    }
    HRESULT hr;
    ptrdiff_t i = m_u8MixerSurfaceCount - 1;
    do {
        ASSERT(!m_apVideoTexture[i] && !m_apVideoSurface[i]);
        if (FAILED(hr = m_pD3DDev->CreateTexture(m_u32VideoWidth, m_u32VideoHeight, 1, D3DUSAGE_RENDERTARGET, dfMixerSurfaceType, D3DPOOL_DEFAULT, &m_apVideoTexture[i], nullptr))) {
            ErrBox(hr, L"creation of video texture failed\n");
        }
        if (FAILED(hr = m_apVideoTexture[i]->GetSurfaceLevel(0, &m_apVideoSurface[i]))) {
            ErrBox(hr, L"loading surface from video texture failed\n");
        }
    } while (--i >= 0);
}

__declspec(nothrow noalias noinline) void CDX9AllocatorPresenter::DeleteSurfaces()
{
    // cleanup of all video-sized surfaces and textures, use this only under safe conditions for the video mixer
    if (m_apTempVideoSurface[0]) {
        m_apTempVideoSurface[0]->Release();
        m_apTempVideoSurface[0] = nullptr;
        m_apTempVideoTexture[0]->Release();
        m_apTempVideoTexture[0] = nullptr;
    }
    if (m_apTempVideoSurface[1]) {
        m_apTempVideoSurface[1]->Release();
        m_apTempVideoSurface[1] = nullptr;
        m_apTempVideoTexture[1]->Release();
        m_apTempVideoTexture[1] = nullptr;
    }
    ptrdiff_t i = MAX_VIDEO_SURFACES - 1;
    do {
        if (m_apVideoSurface[i]) {
            m_apVideoSurface[i]->Release();
            m_apVideoSurface[i] = nullptr;
            m_apVideoTexture[i]->Release();
            m_apVideoTexture[i] = nullptr;
        }
    } while (--i >= 0);
}

__declspec(nothrow noalias) __int64 CDX9AllocatorPresenter::GetVBlank(__in unsigned __int8 u8VBFlags)
{
    LARGE_INTEGER liPerf;
    if (u8VBFlags & FVBLANK_MEASURESTATS) {
        EXECUTE_ASSERT(QueryPerformanceCounter(&liPerf));
    }

    LARGE_INTEGER liVBlank;// BOOL in Vblank low, scanline number high
    static_assert(sizeof(D3DRASTER_STATUS) == 8, "struct D3DRASTER_STATUS or platform settings changed");
    HRESULT hr;
    if (FAILED(hr = m_pD3DDev->GetRasterStatus(0, reinterpret_cast<D3DRASTER_STATUS*>(&liVBlank)))) {
        ErrBox(hr, L"VSync GetRasterStatus failed\n");
    }
    __int32 ScanLine = liVBlank.HighPart * m_i32MonitorHeight / static_cast<__int32>(m_dpPParam.BackBufferHeight);

    if (u8VBFlags & FVBLANK_MEASURESTATS) {
        if (m_i32VBlankMax < ScanLine) {
            m_i32VBlankMax = ScanLine;
        }
        m_i32VBlankMin = m_i32VBlankMax - m_i32MonitorHeight;
    }

    if (liVBlank.LowPart) {// in VBlank
        ScanLine = 0;
    } else if (m_i32VBlankMin != 0x3FF00000) {
        ScanLine -= m_i32VBlankMin;
    }
    liVBlank.HighPart = ScanLine;

    if (u8VBFlags & FVBLANK_MEASURESTATS) {
        EXECUTE_ASSERT(QueryPerformanceCounter(&m_liLastPerfCnt));
        double dTime = static_cast<double>(m_liLastPerfCnt.QuadPart - liPerf.QuadPart) * m_dPerfFreqr;
#ifdef _DEBUG
        if (dTime > 0.5) {
            TRACE(L"Video renderer GetVBlank() too long: %f s\n", dTime);
        }
#endif
        if (m_dRasterStatusWaitTimeMaxCalc < dTime) {
            m_dRasterStatusWaitTimeMaxCalc = dTime;
        }
    }

    return liVBlank.QuadPart;
}

__declspec(nothrow noalias) __int64 CDX9AllocatorPresenter::WaitForVBlankRange(__in __int32 i32RasterStart, __in __int32 i32RasterSize, __in unsigned __int8 u8VBFlags)
{
    __int32 sHalfMonH = static_cast<unsigned __int32>(m_i32MonitorHeight) >> 1;
    LARGE_INTEGER liRasterStatusInitTime;
    if (u8VBFlags & FVBLANK_MEASURESTATS) {
        m_dRasterStatusWaitTimeMaxCalc = 0.0;
        EXECUTE_ASSERT(QueryPerformanceCounter(&liRasterStatusInitTime));
    }

    LARGE_INTEGER liVBlank;// BOOL in Vblank low, scanline number high
    liVBlank.QuadPart = GetVBlank(u8VBFlags);
    if (u8VBFlags & FVBLANK_MEASURESTATS) {
        m_i32VBlankStartWait = liVBlank.HighPart;
    }

    if (m_u32InitialVSyncWait && (u8VBFlags & FVBLANK_MEASURESTATS)) {
        m_u32InitialVSyncWait = 0;
        // If we are already in the wanted interval we need to wait until we aren't, this improves sync when for example you are playing 24/1.001 Hz material on a 24 Hz refresh rate
        __int8 i8InVBlank = 0;
        for (;;) {
            liVBlank.QuadPart = GetVBlank(u8VBFlags);
            if (liVBlank.LowPart && !i8InVBlank) {
                i8InVBlank = 1;
            } else if (!liVBlank.LowPart && i8InVBlank == 1) {
                i8InVBlank = 2;
            } else if (liVBlank.LowPart && i8InVBlank == 2) {
                i8InVBlank = 3;
            } else if (!liVBlank.LowPart && i8InVBlank == 3) {
                break;
            }
        }
    }
    if (u8VBFlags & FVBLANK_WAITIFINSIDE) {
        __int32 ScanLineDiff = liVBlank.HighPart - i32RasterStart;
        if (ScanLineDiff > sHalfMonH) {
            ScanLineDiff -= m_i32MonitorHeight;
        } else if (ScanLineDiff < -sHalfMonH) {
            ScanLineDiff += m_i32MonitorHeight;
        }

        if ((ScanLineDiff >= 0) && (ScanLineDiff <= i32RasterSize)) {
            u8VBFlags |= FVBLANK_WAITED;
            // If we are already in the wanted interval we need to wait until we aren't, this improves sync when for example you are playing 24/1.001 Hz material on a 24 Hz refresh rate
            __int32 LastLineDiff = ScanLineDiff;
            for (;;) {
                liVBlank.QuadPart = GetVBlank(u8VBFlags);

                ScanLineDiff = liVBlank.HighPart - i32RasterStart;
                if (ScanLineDiff > sHalfMonH) {
                    ScanLineDiff -= m_i32MonitorHeight;
                } else if (ScanLineDiff < -sHalfMonH) {
                    ScanLineDiff += m_i32MonitorHeight;
                }

                if ((ScanLineDiff < 0 && ScanLineDiff > i32RasterSize) || (LastLineDiff < 0 && ScanLineDiff > 0)) {
                    break;
                }
                LastLineDiff = ScanLineDiff;
                Sleep(1);// Just sleep
            }
        }
    }
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
    __m128d xSF = _mm_load1_pd(&m_dDetectedScanlinesPerFrame);
    __m128d x0 = _mm_mul_sd(xSF, *reinterpret_cast<__m128d*>(&m_dDetectedRefreshRate));// should do mulsd xmm m64
    static __declspec(align(16)) double const adP1[2] = {0.0015, 1.0 / 3.0};
    __m128d xa = _mm_mul_pd(x0, *reinterpret_cast<__m128d const*>(adP1));// output is: low (a) 1.5 ms, high (b) 33% of Time
    __m128d xd = _mm_set_sd(0.005);
    xd = _mm_mul_sd(xd, x0);// 5 ms
    __m128d xb = _mm_shuffle_pd(xd, xa, _MM_SHUFFLE2(0, 0));// xa low to high, xd low to low
    __m128d xc = _mm_min_pd(xa, xb);
    static __declspec(align(16)) double const adP3[2] = {5.0, 5.0};
    __m128d x4 = _mm_max_pd(xc, *reinterpret_cast<__m128d const*>(adP3));

    ASSERT(_MM_GET_ROUNDING_MODE() == _MM_ROUND_NEAREST);
    __int32 MinRange = _mm_cvtsd_si32(x4);// rounding cast value to __int32
    x4 = _mm_shuffle_pd(x4, x4, _MM_SHUFFLE2(1, 1));// move high to low double
    __int32 MinRange2 = _mm_cvtsd_si32(x4);
#else
    double a = 0.0015 * m_dDetectedScanlinesPerFrame * m_dDetectedRefreshRate;// 1.5 ms
    double b = m_dDetectedScanlinesPerFrame * (1.0 / 3.0);// 33% of Time
    double c = (a < b) ? a : b;
    double d = 0.005 * m_dDetectedScanlinesPerFrame * m_dDetectedRefreshRate;// 5 ms
    double e = (d < b) ? d : b;
    __int32 MinRange = (c > 5.0) ? static_cast<__int32>(c + 0.5) : 5;// 1.5 ms or max 33% of Time
    __int32 MinRange2 = (e > 5.0) ? static_cast<__int32>(e + 0.5) : 5;// 5 ms or max 33% of Time
#endif

    __int32 NoSleepStart = i32RasterStart - MinRange;
    __int32 NoSleepRange = MinRange;
    if (NoSleepStart < 0) {
        NoSleepStart += m_i32MonitorHeight;
    }

    __int32 D3DDevLockStart = i32RasterStart - MinRange2;
    __int32 D3DDevLockRange = MinRange2;
    if (D3DDevLockStart < 0) {
        D3DDevLockStart += m_i32MonitorHeight;
    }

    __int32 ScanLineDiff = liVBlank.HighPart - i32RasterStart;
    if (ScanLineDiff > sHalfMonH) {
        ScanLineDiff -= m_i32MonitorHeight;
    } else if (ScanLineDiff < -sHalfMonH) {
        ScanLineDiff += m_i32MonitorHeight;
    }
    __int32 LastLineDiff = ScanLineDiff;

    __int32 ScanLineDiffSleep = liVBlank.HighPart - NoSleepStart;
    if (ScanLineDiffSleep > sHalfMonH) {
        ScanLineDiffSleep -= m_i32MonitorHeight;
    } else if (ScanLineDiffSleep < -sHalfMonH) {
        ScanLineDiffSleep += m_i32MonitorHeight;
    }
    __int32 LastLineDiffSleep = ScanLineDiffSleep;

    __int32 ScanLineDiffLock = liVBlank.HighPart - D3DDevLockStart;
    if (ScanLineDiffLock > sHalfMonH) {
        ScanLineDiffLock -= m_i32MonitorHeight;
    } else if (ScanLineDiffLock < -sHalfMonH) {
        ScanLineDiffLock += m_i32MonitorHeight;
    }
    __int32 LastLineDiffLock = ScanLineDiffLock;

    LARGE_INTEGER liPerfLock;
    for (;;) {
        liVBlank.QuadPart = GetVBlank(u8VBFlags);

        ScanLineDiff = liVBlank.HighPart - i32RasterStart;
        if (ScanLineDiff > sHalfMonH) {
            ScanLineDiff -= m_i32MonitorHeight;
        } else if (ScanLineDiff < -sHalfMonH) {
            ScanLineDiff += m_i32MonitorHeight;
        }

        if (((ScanLineDiff >= 0) && (ScanLineDiff <= i32RasterSize)) || (LastLineDiff < 0 && ScanLineDiff > 0)) {
            break;
        }

        LastLineDiff = ScanLineDiff;
        u8VBFlags |= FVBLANK_WAITED;

        ScanLineDiffLock = liVBlank.HighPart - D3DDevLockStart;
        if (ScanLineDiffLock > sHalfMonH) {
            ScanLineDiffLock -= m_i32MonitorHeight;
        } else if (ScanLineDiffLock < -sHalfMonH) {
            ScanLineDiffLock += m_i32MonitorHeight;
        }

        if ((((ScanLineDiffLock >= 0) && (ScanLineDiffLock <= D3DDevLockRange)) || ((LastLineDiffLock < 0) && (ScanLineDiffLock > 0)))) {
            if (!(u8VBFlags & FVBLANK_TAKEND3DLOCK) && (u8VBFlags & FVBLANK_MEASURESTATS)) {
                EXECUTE_ASSERT(QueryPerformanceCounter(&liPerfLock));
                // lock D3D device, this method is not official, if it no longer works, so be it
                _RTL_CRITICAL_SECTION* pCritSec = reinterpret_cast<_RTL_CRITICAL_SECTION*>(reinterpret_cast<uintptr_t>(m_pD3DDev) + sizeof(uintptr_t));
                __try {
                    if (pCritSec->DebugInfo->CriticalSection != pCritSec) {
                        ASSERT(0);
                        goto DoNotLock;
                    }
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    // the easy way to catch an access violation if pCritSec or DebugInfo turn out to be invalid pointers
                    // note that the section inside the __try block can only generate an access violation, not any of the other types of errors, those would require an additional set of handlers
                    ASSERT(0);
                    goto DoNotLock;
                }
                EnterCriticalSection(pCritSec);
                u8VBFlags |= FVBLANK_TAKEND3DLOCK;
            }
        }
DoNotLock:
        LastLineDiffLock = ScanLineDiffLock;

        ScanLineDiffSleep = liVBlank.HighPart - NoSleepStart;
        if (ScanLineDiffSleep > sHalfMonH) {
            ScanLineDiffSleep -= m_i32MonitorHeight;
        } else if (ScanLineDiffSleep < -sHalfMonH) {
            ScanLineDiffSleep += m_i32MonitorHeight;
        }

        if (!(((ScanLineDiffSleep >= 0) && (ScanLineDiffSleep <= NoSleepRange)) || ((LastLineDiffSleep < 0) && (ScanLineDiffSleep > 0)))) {
            //TRACE(L"Video renderer active scan line: %d\n", RasterStatus.ScanLine);
            Sleep(1);// Don't sleep for the last 1.5 ms scan lines, so we get maximum precision
        }
        LastLineDiffSleep = ScanLineDiffSleep;
    }

    if (u8VBFlags & FVBLANK_MEASURESTATS) {
        EXECUTE_ASSERT(QueryPerformanceCounter(&m_liLastPerfCnt));
        m_dVBlankWaitTime = static_cast<double>(m_liLastPerfCnt.QuadPart - liRasterStatusInitTime.QuadPart) * m_dPerfFreqr;
        m_i32VBlankEndWait = liVBlank.HighPart;

        double dVBlankLockTime = 0.0;
        if (u8VBFlags & FVBLANK_TAKEND3DLOCK) {
            dVBlankLockTime = static_cast<double>(m_liLastPerfCnt.QuadPart - liPerfLock.QuadPart) * m_dPerfFreqr;
        }
        m_dVBlankLockTime = dVBlankLockTime;

        m_dRasterStatusWaitTime = m_dRasterStatusWaitTimeMaxCalc;
        if (m_dRasterStatusWaitTimeMin > m_dRasterStatusWaitTime) {
            m_dRasterStatusWaitTimeMin = m_dRasterStatusWaitTime;
        }
        if (m_dRasterStatusWaitTimeMax < m_dRasterStatusWaitTime) {
            m_dRasterStatusWaitTimeMax = m_dRasterStatusWaitTime;
        }
    }

    liVBlank.LowPart = u8VBFlags;
    return liVBlank.QuadPart;
}

// Television standards defined their own color spaces, so this is not the precise CIE D65 illuminant but the rounded values from the ITU and EBU standards.
static cmsCIExyY const kxyYwpForNTSCPALSECAMandHD = {0.3127, 0.3290, 1.0};
// static cmsCIExyY const kwpForDigitalCinema = {0.314, 0.351, 1.0};
static cmsCIExyYTRIPLE const kTxyYidentity = {
    {1.0, 0.0, 1.0},
    {0.0, 1.0, 1.0},
    {0.0, 0.0, 1.0}
};
static cmsViewingConditions const ViewingConditionsStudio = {{31.27 / 0.3290, 100.0, (100.0 - 31.27) / 0.3290 - 100.0}, 18.0, 100.0, DIM_SURROUND, 0.0};// in between the 80 cd/m² EBU standard and 120 cd/m² SMPTE standard
// For more information about the recommanded TRC, see the paper at http://www.poynton.com/notes/PU-PR-IS/Poynton-PU-PR-IS.pdf
// CAUTION: these are only random guess values. It requres further testing and fine-tuning (if they will ever work).
#if AMBIENT_LIGHT_DARK != 3 || AMBIENT_LIGHT_DIM != 2 || AMBIENT_LIGHT_OFFICE != 1 || AMBIENT_LIGHT_BRIGHT != 0
#error renderer settings defines changed, edit this section of the code to correct this issue
#endif
static cmsViewingConditions const akViewingConditions[4] = {
    {{31.27 / 0.3290, 100.0, (100.0 - 31.27) / 0.3290 - 100.0}, 10.0, 400.0, AVG_SURROUND, 0.0},// bright
    {{31.27 / 0.3290, 100.0, (100.0 - 31.27) / 0.3290 - 100.0}, 5.0, 240.0, AVG_SURROUND, 0.0},// office
    {{31.27 / 0.3290, 100.0, (100.0 - 31.27) / 0.3290 - 100.0}, 20.0, 40.0, DIM_SURROUND, 0.0},// dim
    {{31.27 / 0.3290, 100.0, (100.0 - 31.27) / 0.3290 - 100.0}, 20.0, 1.0, DARK_SURROUND, 0.0}// dark
};

struct COLORTRANSFORM {double const* pRGBtoXYZlookups; cmsHANDLE hCIECAM02F, hCIECAM02R; cmsHTRANSFORM hTransformXYZtoRGB; void* pOutput; unsigned __int32 u32LookupQuality, u32BlueSliceStart;};

static __declspec(nothrow noalias noinline) DWORD WINAPI ColorMThreadStatic(__in LPVOID lpParam)
{
    ASSERT(lpParam);

    COLORTRANSFORM const* const ptf = reinterpret_cast<COLORTRANSFORM const*>(lpParam);
    cmsHANDLE hCIECAM02F = ptf->hCIECAM02F;
    cmsHANDLE hCIECAM02R = ptf->hCIECAM02R;
    cmsHTRANSFORM hTransformXYZtoRGB = ptf->hTransformXYZtoRGB;
    __int8* pOutput = reinterpret_cast<__int8*>(ptf->pOutput);
    double const* pRGBtoXYZlookups = ptf->pRGBtoXYZlookups;
    unsigned __int32 u32LookupQuality = ptf->u32LookupQuality;
    double const* pMb = pRGBtoXYZlookups + 1536 + static_cast<size_t>(ptf->u32BlueSliceStart);
    unsigned __int8 ib = static_cast<unsigned __int8>(u32LookupQuality >> 4);// a slice is one sixteenth, u32LookupQuality is 256 or less
    do {
        double Xb = pMb[0], Yb = pMb[1], Zb = pMb[2];
        double const* pMg = pRGBtoXYZlookups + 768;
        unsigned __int32 ig = u32LookupQuality;
        do {
            double Xgb = pMg[0] + Xb, Ygb = pMg[1] + Yb, Zgb = pMg[2] + Zb;
            double const* pMr = pRGBtoXYZlookups;
            unsigned __int32 ir = u32LookupQuality;
            do {
                cmsCIEXYZ In = {pMr[0] + Xgb, pMr[1] + Ygb, pMr[2] + Zgb};
                cmsJCh Out;
                cmsCIECAM02Forward(hCIECAM02F, &In, &Out);
                cmsCIECAM02Reverse(hCIECAM02R, &Out, &In);// CIECAM02 transforms (XYZ->Jch->XYZ)
                cmsDoTransform(hTransformXYZtoRGB, &In, pOutput, 1);// XYZ->display RGB transform
                pOutput += 8;
                pMr += 3;
            } while (--ir);
            pMg += 3;
        } while (--ig);
        pMb += 3;
    } while (--ib);
    return 0;
}

struct SHADERTRANSFORM {CDX9AllocatorPresenter::D3DCompilePtr fnD3DCompile; D3D_SHADER_MACRO const* macros; LPCSTR pProfile; char const* szShaderCode; unsigned __int32 u32ShaderLength; IDirect3DDevice9* pD3DDev; IDirect3DPixelShader9** ppShader;};

static __declspec(nothrow noalias noinline) DWORD WINAPI ShadCThreadStatic(__in LPVOID lpParam)
{
    ASSERT(lpParam);

    SHADERTRANSFORM const* const ptf = reinterpret_cast<SHADERTRANSFORM const*>(lpParam);
    // compile a shader
    HRESULT hr;
    ID3DBlob* pD3DBlob;
    if (SUCCEEDED(hr = ptf->fnD3DCompile(ptf->szShaderCode, ptf->u32ShaderLength, nullptr, ptf->macros, nullptr, "main", ptf->pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &pD3DBlob, nullptr))) {
        hr = ptf->pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(pD3DBlob->GetBufferPointer()), ptf->ppShader);
        ASSERT(hr == S_OK);
        pD3DBlob->Release();
    } else {
        ASSERT(0);
    }
    return 0;
}

static struct STATSSCREENCOLORS {__int32 background, white, gray, black, red, green, blue, yellow, cyan, magenta;} const gk_sscColorsets[2] = {
    {0xAF000000, 0xFFFFFFFF, 0xFF7F7F7F, 0xFF000000, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFF00, 0xFF00FFFF, 0xFFFF00FF},// for when working on full range surfaces
    {0xAF404040, 0xFFAAAAAA, 0xFF757575, 0xFF404040, 0xFFAA0000, 0xFF00AA00, 0xFF0000BF, 0xFFAAAA00, 0xFF00AAAA, 0xFFAA00AA}// for when working on limited range surfaces
};

__declspec(nothrow noalias noinline) void CDX9AllocatorPresenter::Paint(__in unsigned __int8 u8RPFlags)
{
    CAutoLock cRenderLock(&m_csRenderLock);

    if (m_pRenderersData->m_bResetStats) {
        m_pRenderersData->m_bResetStats = false;
        m_dRasterStatusWaitTimeMin = 1.0;
        m_dPaintTimeMin = 1.0;
        m_dPaintTimeMax = 0.0;
        m_dRasterStatusWaitTimeMax = 0.0;
        m_dMaxQueueDepth = 0.0;
        if (mk_bIsEVR) {
            static_cast<CEVRAllocatorPresenter*>(this)->ResetStats();
        }
    }

    if (!m_pBackBuffer || !m_apVideoSurface[0]) {// both or either can be nullptr during resets, it varies in different stages of configuring the renderer device and setting up the mixer
        if ((u8RPFlags & FRENDERPAINT_NORMAL) && mk_bIsEVR) {
            static_cast<CEVRAllocatorPresenter*>(this)->DropFrame();
        }
        return;// discard paint calls when a reset is in processing
    }

    EXECUTE_ASSERT(QueryPerformanceCounter(&m_liLastPerfCnt));
    double dStartPaint = static_cast<double>(m_liLastPerfCnt.QuadPart - m_i64PerfCntInit) * m_dPerfFreqr;
    HRESULT hr;
    // frame position on the jitter graph
    size_t upCurrJitterPos = m_upDetectedFrameTimePos & (NB_JITTER - 1);// modulo action by low bitmask

    // validate u8RPFlags for statistics
    if (u8RPFlags & FRENDERPAINT_NORMAL) {// skip extra functions when not redrawing all, detect interruptions
        if (mk_bIsEVR) {
            // note: m_dLastFrameDuration is EVR-only, and it often has very low precision, it's the recorded per frame timing data
            m_adDetectedFrameTimeHistory[upCurrJitterPos] = m_dLastFrameDuration;
        } else {
            double dPaintTimeDiff = dStartPaint - m_dPrevStartPaint;
            // filter out seeking
            if (abs(dPaintTimeDiff) > (8.0 * m_dDetectedVideoTimePerFrame)) {
                m_bDetectedLock = false;
                m_u8SchedulerAdjustTimeOut = 0;
                u8RPFlags = 0;
                goto SkipInitialStats;
            }
            m_adDetectedFrameTimeHistory[upCurrJitterPos] = dPaintTimeDiff;
        }

        size_t upFrames = NB_JITTER - 1;
        double dFrames = 1.0 / NB_JITTER;
        if (m_upDetectedFrameTimePos < NB_JITTER - 1) {
            upFrames = m_upDetectedFrameTimePos;
            dFrames = 1.0 / static_cast<double>(static_cast<__int32>(upFrames + 1));// the standard converter only does a proper job with signed values
        }

        double dDetectedSum = 0.0;
        ptrdiff_t i = upFrames;
        do {
            dDetectedSum += m_adDetectedFrameTimeHistory[i];
        } while (--i >= 0);

        double dAverage = dDetectedSum * dFrames;
        double dDeviationSum = 0.0;
        i = upFrames;
        do {
            double dim = m_adDetectedFrameTimeHistory[i] - dAverage;
            dDeviationSum += dim * dim;
        } while (--i >= 0);

        m_dDetectedFrameTimeStdDev = sqrt(dDeviationSum * dFrames);

        double r = 1.0 / dAverage;
        m_adDetectedFrameTimeRecHistory[upCurrJitterPos] = RoundCommonRates(r);

        // try to lock the frame rate for common rates
        // note: this method is slightly biased towards higher frame rates, because the higher rates have a higher index
        // packed unsigned 16-bit integer values inside 32-bit vectors
        unsigned __int32 r24dAr24 = 0x00010000, r25Ar30d = 0x00030002, r30Ar48d = 0x00050004, r48Ar50 = 0x00070006, r60dAr60 = 0x00090008;// reserve bottom 4 bits for the lookup index, each counter is effectively 12 bits
        i = NB_JITTER - 1;
        do {
            double r = m_adDetectedFrameTimeRecHistory[i];
            if ((r <= 60.0) && (r >= 24.0 / 1.001)) {
                if (r <= 30.0) { // lower set, 24/1.001 to 30 Hz
                    if (r == 24.0 / 1.001) {
                        r24dAr24 += 16;
                    } else if (r == 24.0) {
                        r24dAr24 += 16 << 16;
                    } else if (r == 25.0) {
                        r25Ar30d += 16;
                    } else if (r == 30.0 / 1.001) {
                        r25Ar30d += 16 << 16;
                    } else if (r == 30.0) {
                        r30Ar48d += 16;
                    }
                } else { // higher set, 48/1.001 to 60 Hz
                    if (r == 48.0 / 1.001) {
                        r30Ar48d += 16 << 16;
                    } else if (r == 48.0) {
                        r48Ar50 += 16;
                    } else if (r == 50.0) {
                        r48Ar50 += 16 << 16;
                    } else if (r == 60.0 / 1.001) {
                        r60dAr60 += 16;
                    } else if (r == 60.0) {
                        r60dAr60 += 16 << 16;
                    }
                }
            }
        } while (--i >= 0);
        // try to accommodate parallel processing
        unsigned __int32 nn = r24dAr24 & 0xFFFF, mm = r25Ar30d & 0xFFFF, ll = r30Ar48d & 0xFFFF, kk = r48Ar50 & 0xFFFF, jj = r60dAr60 & 0xFFFF;
        r24dAr24 >>= 16;
        r25Ar30d >>= 16;
        r30Ar48d >>= 16;
        r48Ar50 >>= 16;
        r60dAr60 >>= 16;
        if (nn < r24dAr24) {
            nn = r24dAr24;
        }
        if (mm < r25Ar30d) {
            mm = r25Ar30d;
        }
        if (ll < r30Ar48d) {
            ll = r30Ar48d;
        }
        if (kk < r48Ar50) {
            kk = r48Ar50;
        }
        if (jj < r60dAr60) {
            jj = r60dAr60;
        }
        if (mm < nn) {
            mm = nn;
        }
        if (kk < ll) {
            kk = ll;
        }
        if (mm < jj) {
            mm = jj;
        }
        if (mm < kk) {
            mm = kk;
        }

        double const* pdRate = &m_dRoundedStreamReferenceVideoFrameRate;
        bool bDetectedLock = false;
        if (6 * 16 <= mm) {// require a minimum of 6 to allow the lock
            static double const adLockRatesLookup[10] = {24.0 / 1.001, 24.0, 25.0, 30.0 / 1.001, 30.0, 48.0 / 1.001, 48.0, 50.0, 60.0 / 1.001, 60.0};
            size_t offset = mm & 15;// use the bottom 4 bits for the lookup
            ASSERT(offset < 10);
            pdRate = adLockRatesLookup + offset;
            bDetectedLock = true;
        }
        m_bDetectedLock = bDetectedLock;
        double dRate = *pdRate;
        m_dDetectedVideoFrameRate = dRate;
        m_dDetectedVideoTimePerFrame = 1.0 / dRate;
        m_pSubPicQueue->SetFPS(dRate);
    } else {// paused mode activated
        m_bDetectedLock = false;
        m_u8SchedulerAdjustTimeOut = 0;
    }

SkipInitialStats:
    m_dPrevStartPaint = dStartPaint;

    double dVBlankTime = dStartPaint;// just to make sure it's initialized with something remotely sensible
    bool bPresentEx = false;
    if (m_boCompositionEnabled || (!m_dpPParam.Windowed && (m_u8OSVersionMajor >= 6))) {// Vista and newer only
        bPresentEx = true;
        if (m_dpPParam.Windowed && (m_u16OSVersionMinorMajor == 0x600)) {// DWM present handler for Vista
            DWM_PRESENT_PARAMETERS dpp;
            if ((u8RPFlags & (FRENDERPAINT_NORMAL | FRENDERPAINT_INIT)) && (mk_pRendererSettings->iEVRAlternativeScheduler || ((m_dfSurfaceType != D3DFMT_X8R8G8B8) && (m_dcCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0)) && mk_pRendererSettings->iVMR9FrameInterpolation))) {
                if (!m_bExFrameSchedulerActive) {
                    m_bExFrameSchedulerActive = true;
                    // retrieve the global DWM refresh count of the last vsync
                    DWM_TIMING_INFO dti;
                    dti.cbSize = sizeof(dti);
                    hr = m_fnDwmGetCompositionTimingInfo(nullptr, &dti);// note: normally we use the video window handle for this function
                    ASSERT(hr == S_OK);

                    // set up DWM for queuing
                    dpp.cbSize = sizeof(dpp);
                    dpp.fQueue = TRUE;
                    dpp.cRefreshStart = dti.cRefresh + 1;// enable queuing in the next display refresh
                    dpp.cBuffer = 3;
                    dpp.fUseSourceRate = FALSE;
                    dpp.rateSource.uiNumerator = 0;
                    dpp.rateSource.uiDenominator = 0;
                    dpp.cRefreshesPerFrame = 1;
                    dpp.eSampling = DWM_SOURCE_FRAME_SAMPLING_POINT;
                    EXECUTE_ASSERT(S_OK == (hr = m_fnDwmSetPresentParameters(m_dpPParam.hDeviceWindow, &dpp)));
                }
            } else if (m_bExFrameSchedulerActive) {
                m_bExFrameSchedulerActive = false;
                dpp.cbSize = sizeof(dpp);
                dpp.fQueue = FALSE;
                EXECUTE_ASSERT(S_OK == (hr = m_fnDwmSetPresentParameters(m_dpPParam.hDeviceWindow, &dpp)));
            }
        } else {// swap chain present handler
            if ((u8RPFlags & (FRENDERPAINT_NORMAL | FRENDERPAINT_INIT)) && (mk_pRendererSettings->iEVRAlternativeScheduler || ((m_dfSurfaceType != D3DFMT_X8R8G8B8) && mk_pRendererSettings->iVMR9FrameInterpolation))) {
                m_bExFrameSchedulerActive = true;// no other action required here
            } else if (m_bExFrameSchedulerActive) {// abort the queue
                m_bExFrameSchedulerActive = false;
                m_pD3DDev->PresentEx(nullptr, nullptr, nullptr, nullptr, D3DPRESENT_FORCEIMMEDIATE | D3DPRESENT_DONOTFLIP);// no assertion on the output, in the next pass the tests for the output of PresentEx() are properly handled
            }
        }
    }

    // various usage, for analysis of the graph
    double dSyncOffsetCurr = m_adSyncOffset[m_upNextSyncOffsetPos];
    // use an average to be able to deal with single, large spikes in the graph
    double dSyncOffsetAvrg = dSyncOffsetCurr;
    {
        unsigned __int8 i = 31;
        do {
            dSyncOffsetAvrg += m_adSyncOffset[(m_upNextSyncOffsetPos - i) & (NB_JITTER - 1)];// modulo by low bitmask
        } while (--i);
        dSyncOffsetAvrg *= 0.03125;// average of thirty-two
    }

    // present a ready frame
    if (bPresentEx) {// Vista and newer only
        // set the stats for the current frame default as dropped, this also erases these values for when constant frame interpolation is active, as these values will be filled in after the handling of the stats screen
        m_au8PresentCountLog[upCurrJitterPos] = 0;
        m_afFTdividerILog[upCurrJitterPos] = 0.0f;
        m_adJitter[upCurrJitterPos] = 0.0;
        m_adPaintTimeO[upCurrJitterPos] = 0.0;

        // to allow dropping frames
        double dMinusTwoAndAHalfTPF = -2.5 * m_dDetectedVideoTimePerFrame;
        if (m_bSyncStatsAvailable && m_u8SchedulerAdjustTimeOut && (dSyncOffsetCurr <= dMinusTwoAndAHalfTPF) && (dSyncOffsetAvrg <= dMinusTwoAndAHalfTPF)) {// sync stats required, do not allow constant frame dropping
            if (dSyncOffsetCurr <= dMinusTwoAndAHalfTPF + dMinusTwoAndAHalfTPF) {
                TRACE(L"Video renderer is five frame times behind, expect heavy frame dropping\n");
            }
            m_bDetectedLock = false;
            m_u8SchedulerAdjustTimeOut = 0;
            if ((u8RPFlags & FRENDERPAINT_NORMAL) && mk_bIsEVR) {
                static_cast<CEVRAllocatorPresenter*>(this)->DropFrame();
            }
            return;
        }

        if ((u8RPFlags & FRENDERPAINT_NORMAL) && mk_pRendererSettings->iEVRAlternativeScheduler && ((m_dfSurfaceType == D3DFMT_X8R8G8B8) || (m_dcCaps.PixelShaderVersion < D3DPS_VERSION(3, 0)) || !mk_pRendererSettings->iVMR9FrameInterpolation)) {
            ++m_upSchedulerCorrectionCounter;
            if (unsigned __int8 u8SchedulerAdjustTimeOut = m_u8SchedulerAdjustTimeOut + 1) {// increment up to 255
                m_u8SchedulerAdjustTimeOut = u8SchedulerAdjustTimeOut;
            }
            ASSERT(m_u8SchedulerAdjustTimeOut);

            double dFTdividerI = m_dDetectedRefreshRate * m_dDetectedVideoTimePerFrame;// relates to dFrameEnd
            m_afFTdividerILog[upCurrJitterPos] = static_cast<float>(dFTdividerI);
            double dLowP = floor(dFTdividerI);
            __int8 i8LowP = static_cast<__int8>(dFTdividerI);// this is properly truncated, not rounded
            if (i8LowP > 8) {// limitation, only used to prevent a large interruption in case of error
                i8LowP = 8;
            }

            __int8 i8PresentCount = i8LowP;// default, force low
            if (m_u8ForceNextAmount == 2) {// force high
                ++i8PresentCount;
            } else if (!m_u8ForceNextAmount) {
                i8PresentCount = 0;// calculate the amount
                double dFraction = dFTdividerI - dLowP;
                double dFrameStart = static_cast<double>(static_cast<ptrdiff_t>(m_upDetectedFrameTimePos)) * m_dDetectedVideoTimePerFrame;// convert the frame counter to time in seconds
                //double dFrameEnd = dFrameStart + m_dDetectedVideoTimePerFrame;
                double dFTdivider = m_dDetectedVideoFrameRate * m_dDetectedRefreshTime;
                double dScrOffs = dFrameStart * m_dDetectedRefreshRate;
                double dTRest = fmod(dScrOffs, 1.0) * dFTdivider;

                bool bTrack = true;
                if (m_bSyncStatsAvailable && (m_upSchedulerCorrectionCounter > 5)) {// sync stats required
                    double dOneAndAHalfTPF = 1.5 * m_dDetectedVideoTimePerFrame;
                    double dTwoTPF = m_dDetectedVideoTimePerFrame + m_dDetectedVideoTimePerFrame;
                    if ((dOneAndAHalfTPF < dSyncOffsetCurr) && (dOneAndAHalfTPF < dSyncOffsetAvrg)) {
                        bTrack = false;
                        m_upSchedulerCorrectionCounter = 0;
                        if ((dTwoTPF < dSyncOffsetCurr) && (dTwoTPF < dSyncOffsetAvrg)) {// two frame adaptation to correct large glitches
                            dTwoTPF += m_dDetectedVideoTimePerFrame;
                            ++i8PresentCount;
                            if ((dTwoTPF < dSyncOffsetCurr) && (dTwoTPF < dSyncOffsetAvrg)) {// three frame adaptation to correct huge glitches
                                dTwoTPF += m_dDetectedVideoTimePerFrame;
                                ++i8PresentCount;
                                if ((dTwoTPF < dSyncOffsetCurr) && (dTwoTPF < dSyncOffsetAvrg)) {// four frame adaptation to correct gigantic glitches
                                    dTwoTPF += m_dDetectedVideoTimePerFrame;
                                    ++i8PresentCount;
                                    if ((dTwoTPF < dSyncOffsetCurr) && (dTwoTPF < dSyncOffsetAvrg)) {// five frame adaptation to correct colossal glitches
                                        ++i8PresentCount;
                                    }
                                }
                            }
                        } else if (m_u8SchedulerAdjustTimeOut == 255) {// adjust the detected refresh rate a bit when minor adjustments are made
                            m_dDetectedRefreshRate *= 1.0 + 1.0 / 8192.0;
                            m_dDetectedRefreshTime *= 1.0 / (1.0 + 1.0 / 8192.0);
                        }
                        ++i8PresentCount;// force to high in case of drifting
                    } else if ((-dOneAndAHalfTPF > dSyncOffsetCurr) && (-dOneAndAHalfTPF > dSyncOffsetAvrg)) {
                        bTrack = false;
                        m_upSchedulerCorrectionCounter = 0;
                        if ((-dTwoTPF > dSyncOffsetCurr) && (-dTwoTPF > dSyncOffsetAvrg)) {// two frame adaptation to correct large glitches
                            m_u8SchedulerAdjustTimeOut = 0;
                            dTRest += dFTdivider;
                        } else if (m_u8SchedulerAdjustTimeOut == 255) {// adjust the detected refresh rate a bit when minor adjustments are made
                            m_dDetectedRefreshRate *= 1.0 / (1.0 + 1.0 / 8192.0);
                            m_dDetectedRefreshTime *= 1.0 + 1.0 / 8192.0;
                        }
                        dTRest += dFTdivider;// force to low in case of drifting
                    }
                }

                do {// calculate time fraction
                    if (dTRest >= 1.0) {// i8PresentCount can be 0 after this loop, as dTRest can already be 1.0 or larger before this loop (even without the previous optional corrections)
                        break;
                    }
                    ++i8PresentCount;
                    dTRest += dFTdivider;
                } while (i8PresentCount <= 9);// limitation, only used to prevent a large interruption in case of error

                if (bTrack) {
                    if (dFraction >= 0.5) {
                        if (i8LowP + 1 < i8PresentCount) {
                            m_u8ForceNextAmount = 1;// force low
                        }
                    } else if (i8LowP > i8PresentCount) {
                        m_u8ForceNextAmount = 2;// force high
                    }
                }
            }

            m_au8PresentCountLog[upCurrJitterPos] = i8PresentCount;
            --i8PresentCount;
            if (i8PresentCount >= 0) {
                hr = m_pD3DDev->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
                ASSERT(hr == S_OK || hr == S_PRESENT_MODE_CHANGED || hr == S_PRESENT_OCCLUDED);
                if (FAILED(hr)) {
                    goto PresentFailedDoReset;
                }

                --i8PresentCount;
                if (i8PresentCount >= 0) {
                    if ((m_u16OSVersionMinorMajor == 0x600) && m_dpPParam.Windowed) {
                        EXECUTE_ASSERT(S_OK == (hr = m_fnDwmSetDxFrameDuration(m_dpPParam.hDeviceWindow, static_cast<unsigned __int8>(i8PresentCount) + 2)));// Vista DWM: specify the amount of screen refreshes
                    } else do {
                            hr = m_pD3DDev->PresentEx(nullptr, nullptr, nullptr, nullptr, D3DPRESENT_DONOTFLIP);// hold the frame for multiple screen refreshes
                            ASSERT(hr == S_OK || hr == S_PRESENT_MODE_CHANGED || hr == S_PRESENT_OCCLUDED);
                            if (FAILED(hr)) {
                                goto PresentFailedDoReset;
                            }
                        } while (--i8PresentCount >= 0);
                }
            } else if (mk_bIsEVR) {// can drop frames as well, but unlike the more radical version above, this one continues to render as usual
                static_cast<CEVRAllocatorPresenter*>(this)->DropFrame();
            }
        } else {// single present mode
            hr = m_pD3DDev->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
            if (FAILED(hr)) {
                goto PresentFailedDoReset;
            }
        }
    } else {
        unsigned __int8 u8VBFlags;
        if (u8RPFlags & FRENDERPAINT_NORMAL) {// skip extra functions when not redrawing all
            if (m_bAlternativeVSync) {
                // FlushGPUBeforeVSync query object
                if (mk_pRendererSettings->iVMRFlushGPUBeforeVSync) {
                    LARGE_INTEGER liFlushStartTime;
                    ASSERT(!m_pUtilEventQuery);
                    if (SUCCEEDED(m_pD3DDev->CreateQuery(D3DQUERYTYPE_EVENT, &m_pUtilEventQuery))) {
                        m_pUtilEventQuery->Issue(D3DISSUE_END);
                        EXECUTE_ASSERT(QueryPerformanceCounter(&liFlushStartTime));
                        __int64 liFlushMaxTime = liFlushStartTime.QuadPart;
                        if (mk_pRendererSettings->iVMRFlushGPUWait) {
                            liFlushMaxTime += m_u64PerfFreq >> 3;// wait for the flush, but do set a timeout of .125 s
                        }

                        while (S_FALSE == m_pUtilEventQuery->GetData(nullptr, 0, D3DGETDATA_FLUSH)) {
                            EXECUTE_ASSERT(QueryPerformanceCounter(&m_liLastPerfCnt));
                            if (m_liLastPerfCnt.QuadPart > liFlushMaxTime) {
                                break;
                            }
                        }
                        if (mk_pRendererSettings->iVMRFlushGPUWait) {
                            EXECUTE_ASSERT(QueryPerformanceCounter(&m_liLastPerfCnt));
                            m_dWaitForGPUTime = static_cast<double>(m_liLastPerfCnt.QuadPart - liFlushStartTime.QuadPart) * m_dPerfFreqr;
                        }
                        m_pUtilEventQuery->Release();
                        m_pUtilEventQuery = nullptr;
                    }
                }
                // VSync
                __int32 WaitFor = mk_pRendererSettings->iVMR9VSyncOffset;
                LARGE_INTEGER liVBlankRange;// out: unsigned __int8 VBlank flags low, __int32 scanline number at end high
                liVBlankRange.QuadPart = WaitForVBlankRange(WaitFor, 0, FVBLANK_MEASURESTATS);
                u8VBFlags = static_cast<unsigned __int8>(liVBlankRange.LowPart);// save the flags
            }
            // statistics
            EXECUTE_ASSERT(QueryPerformanceCounter(&m_liLastPerfCnt));
            dVBlankTime = static_cast<double>(m_liLastPerfCnt.QuadPart - m_i64PerfCntInit) * m_dPerfFreqr;
        }

        // present the frame
        hr = m_pSwapChain->Present(nullptr, nullptr, nullptr, nullptr, 0);
        if (FAILED(hr)) {
            goto PresentFailedDoReset;
        }

        if ((u8RPFlags & FRENDERPAINT_NORMAL) && m_bAlternativeVSync) {// skip extra functions when not redrawing all
            LARGE_INTEGER liVBlank;// out: BOOL in Vblank low, scanline number high
            liVBlank.QuadPart = GetVBlank(0);
            m_i32VBlankEndPresent = liVBlank.HighPart;
            while (!liVBlank.HighPart) {
                liVBlank.QuadPart = GetVBlank(0);
            }
            m_i32VBlankStartMeasure = liVBlank.HighPart;
            EXECUTE_ASSERT(QueryPerformanceCounter(&m_liLastPerfCnt));
            m_dVBlankStartMeasureTime = static_cast<double>(m_liLastPerfCnt.QuadPart - m_i64PerfCntInit) * m_dPerfFreqr;

            if (u8VBFlags & FVBLANK_TAKEND3DLOCK) {// unlock D3D device
                _RTL_CRITICAL_SECTION* pCritSec = reinterpret_cast<_RTL_CRITICAL_SECTION*>(reinterpret_cast<uintptr_t>(m_pD3DDev) + sizeof(uintptr_t));
                LeaveCriticalSection(pCritSec);
            }
        }
    }

    if ((m_dPrevStartPaint - m_dPrevSettingsCheck) > 0.5) {// check every .5 second, as the device and swapchain really can't be reset in rapid succession
        bool bPendingResetDevice = false;
        // check settings for changes

        if (m_bVMR9AlterativeVSyncCurrent != mk_pRendererSettings->fVMR9AlterativeVSync) {
            TRACE(L"Video renderer reset device event: VSync setting changed\n");
            bPendingResetDevice = true;
            m_bPartialExDeviceReset = true;
        }

        if (!m_dpPParam.Windowed && (mk_pRendererSettings->iVMR9HighColorResolution != m_bHighColorResolutionCurrent)) {
            TRACE(L"Video renderer reset device event: 10-bit RGB output setting changed\n");
            if (m_pDitherTexture) {// re-writing the dither texture is required if quantization changes
                m_pDitherTexture->Release();
                m_pDitherTexture = nullptr;
            }
            bPendingResetDevice = true;
            m_bPartialExDeviceReset = true;
        }

        if ((m_u8OSVersionMajor == 6) && (m_u8OSVersionMinor < 2)) {// Windows 8 doesn't allow disabling desktop composition
            // renew the Desktop Composition status
            if (mk_pRendererSettings->iVMRDisableDesktopComposition) {
                if (!m_bDesktopCompositionDisabled) {
                    m_bDesktopCompositionDisabled = true;
                    m_fnDwmEnableComposition(DWM_EC_DISABLECOMPOSITION);
                }
            } else if (m_bDesktopCompositionDisabled) {
                m_bDesktopCompositionDisabled = false;
                m_fnDwmEnableComposition(DWM_EC_ENABLECOMPOSITION);
            }
            BOOL boCompositionEnabled;
            m_fnDwmIsCompositionEnabled(&boCompositionEnabled);
            if (m_boCompositionEnabled != boCompositionEnabled) {
                m_boCompositionEnabled = boCompositionEnabled;
                if (m_dpPParam.Windowed) {
                    TRACE(L"Video renderer reset device event: desktop composition changed\n");
                    bPendingResetDevice = true;
                    m_bPartialExDeviceReset = true;
                }
            }
        }

        // test for monitor switching
        bool bMonitorChanged = false;
        HMONITOR hMonitor = MonitorFromWindow(m_hVideoWnd, MONITOR_DEFAULTTONEAREST);
        if (m_hCurrentMonitor != hMonitor) {
            m_hCurrentMonitor = hMonitor;
            bMonitorChanged = true;
            // retrieves the monitor EDID info
            ReadDisplay();
            if (m_dpPParam.Windowed == static_cast<BOOL>(mk_pRendererSettings->bD3DFullscreen)) {// opposite booleans
                goto ChangedDisplayAndD3DFS;
            }
            goto ChangedDisplay;
        }

        // test for D3D Fullscreen Mode switching
        if (m_dpPParam.Windowed == static_cast<BOOL>(mk_pRendererSettings->bD3DFullscreen)) {// opposite booleans
ChangedDisplayAndD3DFS:
            TRACE(L"Video renderer reset device event: D3D Fullscreen setting changed\n");
            if (m_pDitherTexture && mk_pRendererSettings->iVMR9HighColorResolution) {// re-writing the dither texture is required if quantization changes
                m_u8VMR9DitheringLevelsCurrent = 127;// to pass the next check list (as 127 is not an option  for this item)
                m_pDitherTexture->Release();
                m_pDitherTexture = nullptr;
            }
            bPendingResetDevice = true;
            m_bPartialExDeviceReset = true;

ChangedDisplay:
            // test if the master display adapter changed
            __declspec(align(8)) D3DADAPTER_IDENTIFIER9 adapterIdentifier;
            if (UINT uiAdapterCount = m_pD3D->GetAdapterCount()) {
                static_assert(sizeof(adapterIdentifier.Description) == 512, "D3DADAPTER_IDENTIFIER9 component size changed");
                INT i = uiAdapterCount - 1;
                // find the selected adapter, note that the GUIDVRendererDevice will be nullptr if the box is unchecked
                if (i && (mk_pRendererSettings->GUIDVRendererDevice[0] || mk_pRendererSettings->GUIDVRendererDevice[1])) {
                    do {
                        if (SUCCEEDED(m_pD3D->GetAdapterIdentifier(i, 0, &adapterIdentifier))
                                && (reinterpret_cast<__int64*>(&adapterIdentifier.DeviceIdentifier)[0] == mk_pRendererSettings->GUIDVRendererDevice[0])
                                && (reinterpret_cast<__int64*>(&adapterIdentifier.DeviceIdentifier)[1] == mk_pRendererSettings->GUIDVRendererDevice[1])) {
                            m_uiCurrentAdapter = i;
                            goto RegularContinue;
                        }
                    } while (--i >= 0);
                    ASSERT(0);
                    ErrBox(0, L"Failed to find the selected render device in the 'Options', 'Output', 'Playback' tab\n");// duplicate string from initialization section
                }

                do {// use the adapter associated with the monitor
                    HMONITOR hAdpMon = m_pD3D->GetAdapterMonitor(i);
                    if ((hAdpMon == m_hCurrentMonitor) && (SUCCEEDED(m_pD3D->GetAdapterIdentifier(i, 0, &adapterIdentifier)))) {
                        m_uiCurrentAdapter = i;
                        goto RegularContinue;
                    }
                } while (--i >= 0);
                ASSERT(0);
                ErrBox(0, L"Failed to select a suitable render device associated with the current monitor, another compatible render device may be present on the current system\n");// duplicate string from initialization section
            }
            ASSERT(0);
            ErrBox(0, L"Failed to find any suitable render device\n");
RegularContinue:// an efficient break from both loops
            // copy adapter description
            size_t j = 0;
            char sc;
            do {
                sc = adapterIdentifier.Description[j];
                if (!sc) {
                    break;
                }
                m_awcD3D9Device[j] = static_cast<wchar_t>(sc);
            } while (++j < MAX_DEVICE_IDENTIFIER_STRING);
            m_upLenstrD3D9Device = j;
            m_pRenderersData->m_dwPCIVendor = adapterIdentifier.VendorId;

            UINT uiMasterAdapterOrdinalTest = m_dcCaps.MasterAdapterOrdinal;
            EXECUTE_ASSERT(S_OK == (hr = m_pD3D->GetDeviceCaps(m_uiCurrentAdapter, D3DDEVTYPE_HAL, &m_dcCaps)));// m_uiCurrentAdapter is validated before this function, the others are nothing special, this function should never fail
            if (m_dcCaps.MasterAdapterOrdinal != uiMasterAdapterOrdinalTest) {
                m_dReferenceRefreshRate = 0.0;// this is done to renew the detected refresh rate after the CreateDeviceEx()/CreateDevice() procedure
                TRACE(L"Video renderer reset device event: master D3D adapter changed\n");
                bPendingResetDevice = true;
                m_bPartialExDeviceReset = false;

                // external pixel shaders; release the base data, as the new device's pixel shader profile level may be different, keep the strings, the reset sequence will release the interface pointers later on
                POSITION pos = m_apCustomPixelShaders[0].GetHeadPosition();
                while (pos) {
                    EXTERNALSHADER& ceShader = m_apCustomPixelShaders[0].GetNext(pos);
                    if (ceShader.pSrcData) {
                        ceShader.pSrcData->Release();
                        ceShader.pSrcData = nullptr;
                    }
                }
                pos = m_apCustomPixelShaders[1].GetHeadPosition();
                while (pos) {
                    EXTERNALSHADER& ceShader = m_apCustomPixelShaders[1].GetNext(pos);
                    if (ceShader.pSrcData) {
                        ceShader.pSrcData->Release();
                        ceShader.pSrcData = nullptr;
                    }
                }
            } else if (bMonitorChanged) {// some minor changes to internal parts
                D3DDISPLAYMODE DMode;
                EXECUTE_ASSERT(S_OK == (hr = m_pD3D->GetAdapterDisplayMode(m_uiCurrentAdapter, &DMode)));
                m_i32MonitorHeight = DMode.Height;
                m_dDetectedScanlinesPerFrame = m_dMonitorHeight = static_cast<double>(m_i32MonitorHeight);
                // read the refresh rate
                if (mk_pRendererSettings->dRefreshRateAdjust == 1.0) {// automatic refresh rate detection mode
                    if ((m_pRenderersData->m_dwPCIVendor == PCIV_ATI) || (m_pRenderersData->m_dwPCIVendor == PCIV_AMD)) {
                        if (SpecificForAMD()) {
                            goto RefreshRateSet;
                        }
                    } else if (m_u8OSVersionMajor >= 6) {// Vista and newer only
                        if (GenericForExMode()) {
                            goto RefreshRateSet;
                        }
                    }
                }
                // manually adapted refresh rate mode, or timing report failed
                INT rr = DMode.RefreshRate;
                double drr = static_cast<double>(rr);// the standard converter only does a proper job with signed values
                drr = mk_pRendererSettings->dRefreshRateAdjust * ((rr == 119 || rr == 89 || rr == 71 || rr == 59 || rr == 47 || rr == 29 || rr == 23) ? (drr + 1.0) * (1.0 / 1.001)/* NTSC adapted, don't include 95*/ : drr);// exact amounts
                m_dDetectedRefreshRate = m_dReferenceRefreshRate = drr;
                m_dDetectedRefreshTime = 1.0 / drr;
RefreshRateSet:

                if (m_u8OSVersionMajor >= 6) {// Vista and newer only
                    UINT uiMaxLatency = 1;
                    if (!m_bAlternativeVSync) {// set the maximum command latency time to 0.1875 s
                        double dELT = m_dDetectedRefreshRate * 0.1875;
                        if (dELT > 20.0) {// maximum allowed
                            dELT = 20.0;
                        }
                        uiMaxLatency = static_cast<INT>(dELT);// the standard converter only does a proper job with signed values
                    }
                    m_pD3DDev->SetMaximumFrameLatency(uiMaxLatency);
                }

                if (mk_pRendererSettings->iVMR9ColorManagementEnable == 1) {// renew the color management's LUT when the display is changing on the same adapter
                    m_u8VMR9ColorManagementAmbientLightCurrent = 127;// to pass the next check list (as 127 is not an option for this item), this doesn't force the final pixel shader to recompile
                    *reinterpret_cast<__int64*>(&m_dPrevSettingsCheck) = 0xFF00000000000000;// set to a big negative value, force re-check settings
                }
            }
        }

        // test if the surface quality option changed
        if (m_u8VMR9SurfacesQuality != mk_pRendererSettings->iVMR9SurfacesQuality) {
            TRACE(L"Video renderer reset device event: Color settings changed\n");
            bPendingResetDevice = true;
            m_bPartialExDeviceReset = false;
        }

        if (bPendingResetDevice) {
            if (m_u8OSVersionMajor < 6) {// partial resets are available for Vista and newer only
                m_bPartialExDeviceReset = false;
            } else if (m_bPartialExDeviceReset) {
                goto PartialResetSequence;
            }
PresentFailedDoReset:
            m_pBackBuffer->Release();
            m_pBackBuffer = nullptr;
            m_pSwapChain->Release();
            m_pSwapChain = nullptr;
            if (mk_bIsEVR && (u8RPFlags & FRENDERPAINT_NORMAL)) {
                static_cast<CEVRAllocatorPresenter*>(this)->DropFrame();
            }
            EXECUTE_ASSERT(PostMessageW(m_hCallbackWnd, WM_RESET_DEVICE, 0, 0));
            return;
        }
    }

    if (!m_pVBuffer || ((m_dPrevStartPaint - m_dPrevSettingsCheck) > 0.5)) {// check every .5 second, to prevent useless checking
        m_dPrevSettingsCheck = m_dPrevStartPaint;// update the timer

        // check whether the swap chain needs to be reset
        if (m_dpPParam.Windowed) {
            __int32 i32WindowWidth, i32WindowHeight;
            {
                MONITORINFO mi;
                mi.cbSize = sizeof(mi);
                EXECUTE_ASSERT(GetMonitorInfoW(m_hCurrentMonitor, &mi));
                EXECUTE_ASSERT(GetWindowRect(m_hVideoWnd, &mi.rcMonitor));// not used otherwise
                i32WindowWidth = mi.rcMonitor.right - mi.rcMonitor.left;
                i32WindowHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
                // these two are zeroed out in CreateDevice() by default, which is correct for the fullscreen mode
                m_fWindowOnMonitorPosLeft = static_cast<float>(mi.rcMonitor.left) - static_cast<float>(mi.rcWork.left);
                m_fWindowOnMonitorPosTop = static_cast<float>(mi.rcMonitor.top) - static_cast<float>(mi.rcWork.top);
            }

            if (i32WindowWidth && i32WindowHeight) {// will be 0 if minimized
                // TODO: get rid of this blob of code and m_bFullscreenToolBarDetected
                // this part is only to compensate for the control bar of the windowed fullscreen mode, as it pushes the video window aside when unhiding
                // so really, fix that control bar; a window layering problem should not need such a dirty fix in a renderer class
                // compensation for other invading windows (such as the shader editor window) is not provided by this dirty fix
                if (m_u32WindowWidth != m_dpPParam.BackBufferWidth) {// back buffer size changed, this should be changed to also trigger on any changes to height
                    goto PartialResetSequence;
                }

                if (m_bFullscreenToolBarDetected && (static_cast<__int32>(m_dpPParam.BackBufferHeight) == i32WindowHeight)) {// recover from invading toolbar
                    m_bFullscreenToolBarDetected = false;
                    if (m_hUtilityWnd && IsWindowVisible(m_hUtilityWnd)) {// we can't work on the window if it's minimized, or otherwise made invisible
                        EXECUTE_ASSERT(SetWindowPos(m_hUtilityWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOCOPYBITS | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING | SWP_DEFERERASE));// a lot of flags to indicate to only use the X and Y position parameters
                    }
                    goto SkipPartialResetSequence;
                }

                if (static_cast<__int32>(m_dpPParam.BackBufferHeight) != i32WindowHeight) {
                    if (static_cast<__int32>(m_dpPParam.BackBufferHeight) == static_cast<__int32>(m_u32WindowHeight)) {
                        if (!m_bFullscreenToolBarDetected) {// bend over backwards to make the toolbar visible
                            m_bFullscreenToolBarDetected = true;
                            if (m_hUtilityWnd && IsWindowVisible(m_hUtilityWnd)) {// we can't work on the window if it's minimized, or otherwise made invisible
                                EXECUTE_ASSERT(SetWindowPos(m_hUtilityWnd, HWND_TOP, 0, i32WindowHeight - static_cast<__int32>(m_dpPParam.BackBufferHeight), 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOCOPYBITS | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING | SWP_DEFERERASE));// a lot of flags to indicate to only use the X and Y position parameters
                            }
                        }
                    } else {
PartialResetSequence:
                        // note: m_bFullscreenToolBarDetected wil be zeroed out in this sequence
                        // release all screen size dependant items, in about the same order as present in the class
                        if (m_pFont) {
                            m_pFont->Release();
                            m_pFont = nullptr;
                        }
                        if (m_pOSDTexture) {
                            m_pOSDTexture->Release();
                            m_pOSDTexture = nullptr;
                        }
                        if (m_pSubtitleTexture) {
                            m_pSubtitleTexture->Release();
                            m_pSubtitleTexture = nullptr;
                        }
                        if (m_apTempWindowSurface[0]) {
                            m_apTempWindowSurface[0]->Release();
                            m_apTempWindowSurface[0] = nullptr;
                            m_apTempWindowTexture[0]->Release();
                            m_apTempWindowTexture[0] = nullptr;
                        }
                        if (m_apTempWindowSurface[1]) {
                            m_apTempWindowSurface[1]->Release();
                            m_apTempWindowSurface[1] = nullptr;
                            m_apTempWindowTexture[1]->Release();
                            m_apTempWindowTexture[1] = nullptr;
                        }
                        if (m_apTempWindowSurface[2]) {
                            m_apTempWindowSurface[2]->Release();
                            m_apTempWindowSurface[2] = nullptr;
                            m_apTempWindowSurface[3]->Release();
                            m_apTempWindowSurface[3] = nullptr;
                            m_apTempWindowSurface[4]->Release();
                            m_apTempWindowSurface[4] = nullptr;
                            m_apTempWindowTexture[2]->Release();
                            m_apTempWindowTexture[2] = nullptr;
                            m_apTempWindowTexture[3]->Release();
                            m_apTempWindowTexture[3] = nullptr;
                            m_apTempWindowTexture[4]->Release();
                            m_apTempWindowTexture[4] = nullptr;
                            if (m_apFIPreSurface[0]) {
                                m_apFIPreSurface[0]->Release();
                                m_apFIPreSurface[0] = nullptr;
                                m_apFIPreSurface[1]->Release();
                                m_apFIPreSurface[1] = nullptr;
                                m_apFIPreSurface[2]->Release();
                                m_apFIPreSurface[2] = nullptr;
                                m_apFIPreSurface[3]->Release();
                                m_apFIPreSurface[3] = nullptr;
                                m_apFIPreTexture[0]->Release();
                                m_apFIPreTexture[0] = nullptr;
                                m_apFIPreTexture[1]->Release();
                                m_apFIPreTexture[1] = nullptr;
                                m_apFIPreTexture[2]->Release();
                                m_apFIPreTexture[2] = nullptr;
                                m_apFIPreTexture[3]->Release();
                                m_apFIPreTexture[3] = nullptr;
                            }
                        }
                        if (m_pVBuffer) {// reset of the resizer section
                            m_pVBuffer->Release();
                            m_pVBuffer = nullptr;
                            if (m_pResizerPixelShaderX) {
                                m_pResizerPixelShaderX->Release();
                                m_pResizerPixelShaderX = nullptr;
                                if (m_pPreResizerHorizontalPixelShader) {
                                    m_pPreResizerHorizontalPixelShader->Release();
                                    m_pPreResizerHorizontalPixelShader = nullptr;
                                }
                                if (m_pPreResizerVerticalPixelShader) {
                                    m_pPreResizerVerticalPixelShader->Release();
                                    m_pPreResizerVerticalPixelShader = nullptr;
                                }
                                if (m_pResizerPixelShaderY) {
                                    m_pResizerPixelShaderY->Release();
                                    m_pResizerPixelShaderY = nullptr;
                                    m_pIntermediateResizeSurface->Release();
                                    m_pIntermediateResizeSurface = nullptr;
                                    m_pIntermediateResizeTexture->Release();
                                    m_pIntermediateResizeTexture = nullptr;
                                }
                            }
                        }
                        if (m_pStatsRectVBuffer) {
                            m_pStatsRectVBuffer->Release();
                            m_pStatsRectVBuffer = nullptr;
                        }
                        if (m_pFinalPixelShader) {// minor reset of the final pass and the constant frame interpolator
                            m_pFinalPixelShader->Release();
                            m_pFinalPixelShader = nullptr;
                        }
                        if (m_pFIBufferRT0) {
                            m_pFIBufferRT0->Release();
                            m_pFIBufferRT0 = nullptr;
                            if (m_pFIBufferRT1) {
                                m_pFIBufferRT1->Release();
                                m_pFIBufferRT1 = nullptr;
                                if (m_pFIBufferRT2) {
                                    m_pFIBufferRT2->Release();
                                    m_pFIBufferRT2 = nullptr;
                                }
                            }
                        }
                        m_pBackBuffer->Release();
                        m_pBackBuffer = nullptr;
                        m_pSwapChain->Release();
                        m_pSwapChain = nullptr;

                        if (m_u8OSVersionMajor >= 6) {// Vista and newer only, an IDirect3DDevice9Ex device inherits from the implicit swap chain, so a partial device reset is required
                            TRACE(L"Video renderer partial reset of an IDirect3DDevice9Ex device\n");
                            m_bPartialExDeviceReset = true;
                            if ((u8RPFlags & FRENDERPAINT_NORMAL) && mk_bIsEVR) {
                                static_cast<CEVRAllocatorPresenter*>(this)->DropFrame();
                            }
                            EXECUTE_ASSERT(PostMessageW(m_hCallbackWnd, WM_RESET_DEVICE, 0, 0));
                            return;
                        }

                        // window size parts
                        // note: m_u32WindowWidth and m_u32WindowHeight are configured by CSubPicAllocatorPresenterImpl, those are only used to check for resets
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
                        __m128 x2 = _mm_set_ps1(1.0f);
                        static_assert(sizeof(m_dpPParam.BackBufferWidth) == 4, "struct D3DPRESENT_PARAMETERS or platform settings changed");// the struct is declared on a global level
                        __m128i xWS = _mm_loadl_epi64(reinterpret_cast<__m128i*>(&m_u32WindowWidth));
                        __m128d x0 = _mm_cvtepi32_pd(xWS);// __int32 to double
                        _mm_storel_epi64(reinterpret_cast<__m128i*>(&m_dpPParam.BackBufferWidth), xWS);// also stores BackBufferHeight
                        _mm_store_pd(&m_dWindowWidth, x0);// also stores m_dWindowHeight
                        __m128 x1 = _mm_cvtpd_ps(x0);// double to float
                        x2 = _mm_div_ps(x2, x1);// reciprocal trough _mm_rcp_ps() isn't accurate
                        _mm_storel_pi(reinterpret_cast<__m64*>(&m_fWindowWidth), x1);// also stores m_fWindowHeight
                        _mm_storel_pi(reinterpret_cast<__m64*>(&m_fWindowWidthr), x2);// also stores m_fWindowHeightr
#else
                        m_dpPParam.BackBufferWidth = m_u32WindowWidth;
                        m_dpPParam.BackBufferHeight = m_u32WindowHeight;
                        m_dWindowWidth = static_cast<double>(static_cast<__int32>(m_u32WindowWidth));// the standard converter only does a proper job with signed values
                        m_dWindowHeight = static_cast<double>(static_cast<__int32>(m_u32WindowHeight));
                        m_fWindowWidth = static_cast<float>(m_dWindowWidth);
                        m_fWindowHeight = static_cast<float>(m_dWindowHeight);
                        m_fWindowWidthr = 1.0f / m_fWindowWidth;
                        m_fWindowHeightr = 1.0f / m_fWindowHeight;
#endif
                        unsigned __int8 u8Nibble;
                        // m_szWindowWidth; standard method for converting numbers to hex strings
                        ASSERT(m_u32WindowWidth <= 0x9FFFF);// the method implementation limit here
                        u8Nibble = static_cast<unsigned __int8>(m_u32WindowWidth >> 16);// each hexadecimal char stores 4 bits
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

                        // m_szWindowHeight; standard method for converting numbers to hex strings
                        ASSERT(m_u32WindowHeight <= 0x9FFFF);// the method implementation limit here
                        u8Nibble = static_cast<unsigned __int8>(m_u32WindowHeight >> 16);// each hexadecimal char stores 4 bits
                        m_szWindowHeight[2] = '0' + u8Nibble;
                        u8Nibble = (m_u32WindowHeight >> 12) & 15;
                        u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                        m_szWindowHeight[3] = u8Nibble;
                        u8Nibble = (m_u32WindowHeight >> 8) & 15;
                        u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                        m_szWindowHeight[4] = u8Nibble;
                        u8Nibble = (m_u32WindowHeight >> 4) & 15;
                        u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                        m_szWindowHeight[5] = u8Nibble;
                        u8Nibble = m_u32WindowHeight & 15;
                        u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                        m_szWindowHeight[6] = u8Nibble;

                        // non-zero initialize 6 values for VSync functions
                        // 0x3FF00000 is big enough and convienient to use here
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
                        _mm_stream_si32(&m_i32VBlankMin, 0x3FF00000);
                        _mm_stream_si32(&m_i32VBlankEndPresent, 0x3FF00000);
                        _mm_stream_si32(reinterpret_cast<__int32*>(&m_u32InitialVSyncWait), 0x3FF00000);// the value here doesn't matter at all, it's just convienient to re-use here
                        // doubles to 1.0
                        _mm_stream_si32(reinterpret_cast<__int32*>(&m_dPaintTimeMin), 0);
                        _mm_stream_si32(reinterpret_cast<__int32*>(&m_dPaintTimeMin) + 1, 0x3FF00000);
                        _mm_stream_si32(reinterpret_cast<__int32*>(&m_dRasterStatusWaitTimeMin), 0);
                        _mm_stream_si32(reinterpret_cast<__int32*>(&m_dRasterStatusWaitTimeMin) + 1, 0x3FF00000);
                        _mm_stream_si32(reinterpret_cast<__int32*>(&m_dModeratedTimeSpeed), 0);
                        _mm_stream_si32(reinterpret_cast<__int32*>(&m_dModeratedTimeSpeed) + 1, 0x3FF00000);
#else
                        m_i32VBlankMin = 0x3FF00000;
                        m_i32VBlankEndPresent = 0x3FF00000;
                        m_u32InitialVSyncWait = 0x3FF00000;// the value here doesn't matter at all, it's just convienient to re-use here
                        // doubles to 1.0
                        reinterpret_cast<__int32*>(&m_dPaintTimeMin)[0] = 0;
                        reinterpret_cast<__int32*>(&m_dPaintTimeMin)[1] = 0x3FF00000;
                        reinterpret_cast<__int32*>(&m_dRasterStatusWaitTimeMin)[0] = 0;
                        reinterpret_cast<__int32*>(&m_dRasterStatusWaitTimeMin)[1] = 0x3FF00000;
                        reinterpret_cast<__int32*>(&m_dModeratedTimeSpeed)[0] = 0;
                        reinterpret_cast<__int32*>(&m_dModeratedTimeSpeed)[1] = 0x3FF00000;
#endif
                        // zero initialize many values for VSync functions, see header file for reference
                        __m128 xZero = _mm_setzero_ps();// only a command to set a register to zero, this should not add constant value to the assembly
                        static_assert(!(offsetof(CDX9AllocatorPresenter, m_bSyncStatsAvailable) & 15), "structure alignment test failed, edit this class to correct the issue");
                        static_assert(!((offsetof(CDX9AllocatorPresenter, m_dMaxQueueDepth) + sizeof(m_dMaxQueueDepth) - offsetof(CDX9AllocatorPresenter, m_bSyncStatsAvailable)) & 15), "modulo 16 byte count for routine data set test failed, edit this class to correct the issue");
                        unsigned __int32 u32Erase = static_cast<unsigned __int32>((offsetof(CDX9AllocatorPresenter, m_dMaxQueueDepth) + sizeof(m_dMaxQueueDepth) - offsetof(CDX9AllocatorPresenter, m_bSyncStatsAvailable)) >> 4);
                        float* pDst = reinterpret_cast<float*>(&m_bSyncStatsAvailable);
                        do {
                            _mm_stream_ps(pDst, xZero);// zero-fills target
                            pDst += 4;
                        } while (--u32Erase);// 16 aligned bytes are written every time

                        // re-create the windowed swap chain
                        ASSERT(!m_pSwapChain && m_pBackBuffer);
                        m_pD3DDev->CreateAdditionalSwapChain(&m_dpPParam, reinterpret_cast<IDirect3DSwapChain9**>(&m_pSwapChain));// initialize the windowed swap chain, see the header for IDirect3DSwapChain9 and IDirect3DSwapChain9Ex modes for m_pSwapChain
                        m_pSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &m_pBackBuffer);
                        m_pD3DDev->ColorFill(m_pBackBuffer, nullptr, 0);// this is just to make sure that no backbuffers are ever presented uninitialized
                    }
                }
            }
        }
SkipPartialResetSequence:

        // check whether the vertex buffer, resizer pixel shaders and the intermediate resizer surface must be initialized
        if (!m_pVBuffer || (m_u8DX9ResizerTest != mk_pRendererSettings->iDX9Resizer) || (m_xformTest != m_xform) || (m_rcVideoRectTest.left != m_VideoRect.left) || (m_rcVideoRectTest.top != m_VideoRect.top) || (m_rcVideoRectTest.right != m_VideoRect.right) || (m_rcVideoRectTest.bottom != m_VideoRect.bottom)) {
            m_rcVideoRectTest = m_VideoRect;
            m_xformTest = m_xform;
            m_u8DX9ResizerTest = mk_pRendererSettings->iDX9Resizer;

            if (m_pVBuffer) {
                m_pVBuffer->Release();
                m_pVBuffer = nullptr;
                if (m_pResizerPixelShaderX) {
                    m_pResizerPixelShaderX->Release();
                    m_pResizerPixelShaderX = nullptr;
                    if (m_pPreResizerHorizontalPixelShader) {
                        m_pPreResizerHorizontalPixelShader->Release();
                        m_pPreResizerHorizontalPixelShader = nullptr;
                    }
                    if (m_pPreResizerVerticalPixelShader) {
                        m_pPreResizerVerticalPixelShader->Release();
                        m_pPreResizerVerticalPixelShader = nullptr;
                    }
                    if (m_pResizerPixelShaderY) {
                        m_pResizerPixelShaderY->Release();
                        m_pResizerPixelShaderY = nullptr;
                        m_pIntermediateResizeSurface->Release();
                        m_pIntermediateResizeSurface = nullptr;
                        m_pIntermediateResizeTexture->Release();
                        m_pIntermediateResizeTexture = nullptr;
                    }
                }
            }

            // create a vertex buffer
            ASSERT(!m_pVBuffer);
            hr = m_pD3DDev->CreateVertexBuffer(sizeof(CUSTOMVERTEX_TEX1[20]), D3DUSAGE_DONOTCLIP | D3DUSAGE_WRITEONLY, D3DFVF_XYZRHW | D3DFVF_TEX1, D3DPOOL_DEFAULT, &m_pVBuffer, nullptr);
            void* pVoid;
            hr = m_pVBuffer->Lock(0, 0, &pVoid, 0);

            // set up artifact clearing rectangles
            __m128 xZero = _mm_setzero_ps();// only a command to set a register to zero, this should not add constant value to the assembly
            float* pDstRC = reinterpret_cast<float*>(&m_rcClearLeft);
            unsigned __int8 k = 4;// zero the set of four
            do {
                _mm_store_ps(pDstRC, xZero);
                pDstRC += 4;
            } while (--k);

            if (m_VideoRect.left > 0) {
                if (m_VideoRect.top > 0) {
                    m_rcClearLeft.top = m_VideoRect.top;
                }
                m_rcClearLeft.right = m_VideoRect.left;
                m_rcClearLeft.bottom = (m_VideoRect.bottom > static_cast<LONG>(m_dpPParam.BackBufferHeight)) ? m_dpPParam.BackBufferHeight : m_VideoRect.bottom;
            }
            if (m_VideoRect.top > 0) {
                m_rcClearTop.right = m_dpPParam.BackBufferWidth;
                m_rcClearTop.bottom =  m_VideoRect.top;
            }
            if (m_VideoRect.right < static_cast<LONG>(m_dpPParam.BackBufferWidth)) {
                m_rcClearRight.left = m_VideoRect.right;
                if (m_VideoRect.top > 0) {
                    m_rcClearRight.top = m_VideoRect.top;
                }
                m_rcClearRight.right = m_dpPParam.BackBufferWidth;
                m_rcClearRight.bottom = (m_VideoRect.bottom > static_cast<LONG>(m_dpPParam.BackBufferHeight)) ? m_dpPParam.BackBufferHeight : m_VideoRect.bottom;
            }
            if (m_VideoRect.bottom < static_cast<LONG>(m_dpPParam.BackBufferHeight)) {
                m_rcClearBottom.top = m_VideoRect.bottom;
                m_rcClearBottom.right = m_dpPParam.BackBufferWidth;
                m_rcClearBottom.bottom = m_dpPParam.BackBufferHeight;
            }
            m_rcTearing.bottom = m_dpPParam.BackBufferHeight;// initialize the tearing test rectangle

            // test if the input and output size for the resizers is equal, so nearest neighbor can be forced
            ULONG ULVRWidth = m_VideoRect.right - m_VideoRect.left, ULVRHeight = m_VideoRect.bottom - m_VideoRect.top;
            unsigned __int8 u8ActiveResizer = mk_pRendererSettings->iDX9Resizer;
            LPCSTR szYPassWidth = m_szWindowWidth;
            bool bNoHresize = false, bNoVresize = ULVRHeight == m_u32VideoHeight;
            m_bNoVresize = bNoVresize;
            if (ULVRWidth == m_u32VideoWidth) {
                bNoHresize = true;
                szYPassWidth = m_szVideoWidth;
                if (bNoVresize) {
                    u8ActiveResizer = 0;
                }
            }
            m_u8ActiveResizer = u8ActiveResizer;
            m_bNoHresize = bNoHresize;
            // set the resizer pass context-specific width to the appropriate string
            ASSERT((m_aShaderMacros[9].Name[0] == 'M') && (m_aShaderMacros[9].Name[1] == 'e'));
            m_aShaderMacros[9].Definition = szYPassWidth;

            unsigned __int8 u8Nibble;
            // m_szResizedWidth; standard method for converting numbers to hex strings
            ASSERT(ULVRWidth <= 0x9FFFF);// the method implementation limit here
            u8Nibble = static_cast<unsigned __int8>(ULVRWidth >> 16);// each hexadecimal char stores 4 bits
            m_szResizedWidth[2] = '0' + u8Nibble;
            u8Nibble = (ULVRWidth >> 12) & 15;
            u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
            m_szResizedWidth[3] = u8Nibble;
            u8Nibble = (ULVRWidth >> 8) & 15;
            u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
            m_szResizedWidth[4] = u8Nibble;
            u8Nibble = (ULVRWidth >> 4) & 15;
            u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
            m_szResizedWidth[5] = u8Nibble;
            u8Nibble = ULVRWidth & 15;
            u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
            m_szResizedWidth[6] = u8Nibble;

            // m_szResizedHeight; standard method for converting numbers to hex strings
            ASSERT(ULVRHeight <= 0x9FFFF);// the method implementation limit here
            u8Nibble = static_cast<unsigned __int8>(ULVRHeight >> 16);// each hexadecimal char stores 4 bits
            m_szResizedHeight[2] = '0' + u8Nibble;
            u8Nibble = (ULVRHeight >> 12) & 15;
            u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
            m_szResizedHeight[3] = u8Nibble;
            u8Nibble = (ULVRHeight >> 8) & 15;
            u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
            m_szResizedHeight[4] = u8Nibble;
            u8Nibble = (ULVRHeight >> 4) & 15;
            u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
            m_szResizedHeight[5] = u8Nibble;
            u8Nibble = ULVRHeight & 15;
            u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
            m_szResizedHeight[6] = u8Nibble;

            // set up geometry
            double dVRectWidth = static_cast<double>(static_cast<LONG>(ULVRWidth)), dVRectHeight = static_cast<double>(static_cast<LONG>(ULVRHeight)),// the standard converter only does a proper job with signed values
                   ldiv = (1.0 / 1.5) / sqrt(dVRectWidth * dVRectWidth + dVRectHeight * dVRectHeight) + (1.0 / 1.5),
                   vrl = static_cast<double>(m_VideoRect.left), vrt = static_cast<double>(m_VideoRect.top), vrr = static_cast<double>(m_VideoRect.right), vrb = static_cast<double>(m_VideoRect.bottom);
            Vector dst[4] = {Vector(vrl, vrt, 0.0), Vector(vrr, vrt, 0.0), Vector(vrl, vrb, 0.0), Vector(vrr, vrb, 0.0)},
                            center((vrl + vrr) * 0.5, (vrt + vrb) * 0.5, 0.0);

            // write the source and destination RECT structures, used for StretchRect() resizing (nearest neighbor and bilinear)
            double dRw = m_dVideoWidth / dVRectWidth, dRh = m_dVideoHeight / dVRectHeight;
            m_rcResizerStretchRectSrc.left = 0;
            m_rcResizerStretchRectSrc.top = 0;
            m_rcResizerStretchRectSrc.right = m_u32VideoWidth;
            m_rcResizerStretchRectSrc.bottom = m_u32VideoHeight;
            m_rcResizerStretchRectDst = m_VideoRect;
            if (m_VideoRect.left < 0) {
                m_rcResizerStretchRectSrc.left = static_cast<LONG>(-vrl * dRw + 0.5);// all numbers here are made positive, so rounding only by +0.5
                m_rcResizerStretchRectDst.left = 0;
            }
            if (m_VideoRect.top < 0) {
                m_rcResizerStretchRectSrc.top = static_cast<LONG>(-vrt * dRh + 0.5);
                m_rcResizerStretchRectDst.top = 0;
            }
            if (m_VideoRect.right > static_cast<LONG>(m_dpPParam.BackBufferWidth)) {
                m_rcResizerStretchRectSrc.right = static_cast<LONG>(m_dVideoWidth - (dVRectWidth - vrr) * dRw + 0.5);
                m_rcResizerStretchRectDst.right = m_dpPParam.BackBufferWidth;
            }
            if (m_VideoRect.bottom > static_cast<LONG>(m_dpPParam.BackBufferHeight)) {
                m_rcResizerStretchRectSrc.bottom = static_cast<LONG>(m_dVideoHeight - (dVRectHeight - vrb) * dRh + 0.5);
                m_rcResizerStretchRectDst.bottom = m_dpPParam.BackBufferHeight;
            }

            Vector* pVtc = dst;
            unsigned __int8 j = 4;
            do {
                *pVtc = m_xform << (*pVtc - center);
                double zr = pVtc->z * ldiv + 0.5;
                pVtc->z = zr;
                double zdiv = 0.5 / zr;
                pVtc->x = pVtc->x * zdiv - 0.5;
                pVtc->y = pVtc->y * zdiv - 0.5;
                *pVtc += center;
                ++pVtc;
            } while (--j);

            float vidwm = m_fVideoWidth - 0.5f, vidhm = m_fVideoHeight - 0.5f, wndwm = m_fWindowWidth - 0.5f, wndhm = m_fWindowHeight - 0.5f;

            // note: size specification of this array is used for allocating the buffer
            __declspec(align(16)) CUSTOMVERTEX_TEX1 v[20] = {// lists for DrawIndexedPrimitive() with the number used for the BaseVertexIndex item
                { -0.5f, -0.5f, 0.5f, 2.0f, 0.0f, 0.0f},// window size: 0
                {wndwm, -0.5f, 0.5f, 2.0f, 1.0f, 0.0f},
                { -0.5f, wndhm, 0.5f, 2.0f, 0.0f, 1.0f},
                {wndwm, wndhm, 0.5f, 2.0f, 1.0f, 1.0f},
                { -0.5f, -0.5f, 0.5f, 2.0f, 0.0f, 0.0f},// video size: 4
                {vidwm, -0.5f, 0.5f, 2.0f, 1.0f, 0.0f},
                { -0.5f, vidhm, 0.5f, 2.0f, 0.0f, 1.0f},
                {vidwm, vidhm, 0.5f, 2.0f, 1.0f, 1.0f},
                {static_cast<float>(dst[0].x), static_cast<float>(dst[0].y), 0.5f, 2.0f, -0.5f, -0.5f},// 1 pass resizers: 8
                {static_cast<float>(dst[1].x), static_cast<float>(dst[1].y), 0.5f, 2.0f, vidwm, -0.5f},
                {static_cast<float>(dst[2].x), static_cast<float>(dst[2].y), 0.5f, 2.0f, -0.5f, vidhm},
                {static_cast<float>(dst[3].x), static_cast<float>(dst[3].y), 0.5f, 2.0f, vidwm, vidhm},
                {static_cast<float>(dst[0].x), -0.5f, 0.5f, 2.0f, -0.5f, -0.5f},// 2 pass resizers x: 12
                {static_cast<float>(dst[1].x), -0.5f, 0.5f, 2.0f, vidwm, -0.5f},
                {static_cast<float>(dst[2].x), vidhm, 0.5f, 2.0f, -0.5f, vidhm},
                {static_cast<float>(dst[3].x), vidhm, 0.5f, 2.0f, vidwm, vidhm},
                { -0.5f, static_cast<float>(dst[0].y), 0.5f, 2.0f, -0.5f, -0.5f},// 2 pass resizers y: 16
                {wndwm, static_cast<float>(dst[1].y), 0.5f, 2.0f, wndwm, -0.5f},
                { -0.5f, static_cast<float>(dst[2].y), 0.5f, 2.0f, -0.5f, vidhm},
                {wndwm, static_cast<float>(dst[3].y), 0.5f, 2.0f, wndwm, vidhm}
            };

            float* pDst = reinterpret_cast<float*>(pVoid), *pSrc = &v[0].x;
            ASSERT(!(reinterpret_cast<uintptr_t>(pDst) & 15));// if not 16-byte aligned, _mm_stream_ps will fail
            j = 3;// in 3 batches of 16×8 bytes + 16×6 extra
            do {
                __m128 x0 = _mm_load_ps(pSrc);
                __m128 x1 = _mm_load_ps(pSrc + 4);
                __m128 x2 = _mm_load_ps(pSrc + 8);
                __m128 x3 = _mm_load_ps(pSrc + 12);
                __m128 x4 = _mm_load_ps(pSrc + 16);
                __m128 x5 = _mm_load_ps(pSrc + 20);
                __m128 x6 = _mm_load_ps(pSrc + 24);
                __m128 x7 = _mm_load_ps(pSrc + 28);
                pSrc += 32;
                _mm_stream_ps(pDst, x0);
                _mm_stream_ps(pDst + 4, x1);
                _mm_stream_ps(pDst + 8, x2);
                _mm_stream_ps(pDst + 12, x3);
                _mm_stream_ps(pDst + 16, x4);
                _mm_stream_ps(pDst + 20, x5);
                _mm_stream_ps(pDst + 24, x6);
                _mm_stream_ps(pDst + 28, x7);
                pDst += 32;
            } while (--j);
            __m128 x0 = _mm_load_ps(pSrc);
            __m128 x1 = _mm_load_ps(pSrc + 4);
            __m128 x2 = _mm_load_ps(pSrc + 8);
            __m128 x3 = _mm_load_ps(pSrc + 12);
            __m128 x4 = _mm_load_ps(pSrc + 16);
            __m128 x5 = _mm_load_ps(pSrc + 20);
            _mm_stream_ps(pDst, x0);
            _mm_stream_ps(pDst + 4, x1);
            _mm_stream_ps(pDst + 8, x2);
            _mm_stream_ps(pDst + 12, x3);
            _mm_stream_ps(pDst + 16, x4);
            _mm_stream_ps(pDst + 20, x5);

            // store the video area normalized rectangle
            __m128 xWindowSizer = _mm_load_ps(&m_fWindowWidthr);
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
            __m128 xWorkingRect = _mm_cvtepi32_ps(*reinterpret_cast<__m128i*>(&m_VideoRect));
#else
            static __declspec(align(16)) float const kafCastFloatHighBit[4] = {8388608.0f, 8388608.0f, 8388608.0f, 8388608.0f};// 0x4B000000 normalized exponent bits of a float for the interval [1 << 23, 1 << 24), this equals 1 << 23 in floating point, used for both raw exponent bits for orps, as well as to subtract the high bit after that with subps
            static __declspec(align(16)) float const kafCastFloatSecondBit[4] = { -16777216.0f, -16777216.0f, -16777216.0f, -16777216.0f};// used for negative values, it's -2 * 8388608
            static __declspec(align(16)) __int32 const kai32ExponentBitMask[4] = {0x807FFFFF, 0x807FFFFF, 0x807FFFFF, 0x807FFFFF};
            // limitations of the casting mechanism, should never be a problem here
            ASSERT(m_VideoRect.left >= -8388608 && m_VideoRect.left < 8388608);
            ASSERT(m_VideoRect.top >= -8388608 && m_VideoRect.top < 8388608);
            ASSERT(m_VideoRect.right >= -8388608 && m_VideoRect.right < 8388608);
            ASSERT(m_VideoRect.bottom >= -8388608 && m_VideoRect.bottom < 8388608);
            __m128 xCastFloatHighBit = _mm_load_ps(kafCastFloatHighBit);
            __m128 xWorkingRect = _mm_and_ps(*reinterpret_cast<__m128*>(&m_VideoRect), *reinterpret_cast<__m128 const*>(kai32ExponentBitMask));
            xWorkingRect = _mm_or_ps(xWorkingRect, xCastFloatHighBit);// put the exponent bits in place after the loaded integer mantissa
            __m128 xWorkingRectN = _mm_sub_ps(*reinterpret_cast<__m128 const*>(kafCastFloatSecondBit), xWorkingRect);// negative values; subtract from -2 * the implicit high bit, the FPU will auto-normalize the values accordingly
            xWorkingRect = _mm_sub_ps(xWorkingRect, xCastFloatHighBit);// positive values; subtract the implicit high bit, the FPU will auto-normalize the values accordingly
            xWorkingRect = _mm_max_ps(xWorkingRect, xWorkingRectN);// rest assured, no bits were hurt in this conversion procedure (as long as the values were in range)
#endif
            xWindowSizer = _mm_movelh_ps(xWindowSizer, xWindowSizer);// only the lower two (reciprocals) are used
            xWorkingRect = _mm_mul_ps(xWorkingRect, xWindowSizer);// the complete [0, 1] normalized video rectangle relative to the window
            _mm_store_ps(m_afNormRectVideoArea, xWorkingRect);

            hr = m_pVBuffer->Unlock();// I added a little task before this call to keep the CPU busy while memory is still copying.
            // set the vertex state and buffer
            m_pD3DDev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
            if (FAILED(hr = m_pD3DDev->SetStreamSource(0, m_pVBuffer, 0, sizeof(CUSTOMVERTEX_TEX1)))) {
                ErrBox(hr, L"activating the standard vertex buffer failed\n");
            }

            if ((m_u8ActiveResizer > 1) && (!m_bNoHresize || !m_bNoVresize)) {// in some cases, a resizer pixel shader isn't required
                ASSERT(!m_pUtilD3DBlob && !m_pPreResizerHorizontalPixelShader && !m_pPreResizerVerticalPixelShader && !m_pResizerPixelShaderX && !m_pResizerPixelShaderY);

                // create the blur shaders in case of down-sizing
                if (ULVRWidth < m_u32VideoWidth) {
                    if (FAILED(hr = m_fnD3DCompile(gk_szHorizontalBlurShader, gk_u32LenHorizontalBlurShader, nullptr, m_aShaderMacros, nullptr, "main", m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &m_pUtilD3DBlob, nullptr))
                            || FAILED(hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(m_pUtilD3DBlob->GetBufferPointer()), &m_pPreResizerHorizontalPixelShader))) {
                        ErrBox(hr, L"compiling resizer horizontal pre-processing pixel shader failed\n");
                    }
                    m_pUtilD3DBlob->Release();
                    m_pUtilD3DBlob = nullptr;
                }
                if (ULVRHeight < m_u32VideoHeight) {
                    if (FAILED(hr = m_fnD3DCompile(gk_szVerticalBlurShader, gk_u32LenVerticalBlurShader, nullptr, m_aShaderMacros, nullptr, "main", m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &m_pUtilD3DBlob, nullptr))
                            || FAILED(hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(m_pUtilD3DBlob->GetBufferPointer()), &m_pPreResizerVerticalPixelShader))) {
                        ErrBox(hr, L"compiling resizer vertical pre-processing pixel shader failed\n");
                    }
                    m_pUtilD3DBlob->Release();
                    m_pUtilD3DBlob = nullptr;
                }

                // The list for resizers is offset by two; shaders 0 and 1 are never used
                // the named entry for single-pass resizers is always "mainH"
                if (FAILED(hr = m_fnD3DCompile(gk_aszResizerShader[m_u8ActiveResizer - 2], gk_au32LenResizerShader[m_u8ActiveResizer - 2], nullptr, m_aShaderMacros, nullptr, (m_bNoHresize && (m_u8ActiveResizer > 2)) ? "mainV" : "mainH", m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &m_pUtilD3DBlob, nullptr))
                        || FAILED(hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(m_pUtilD3DBlob->GetBufferPointer()), &m_pResizerPixelShaderX))) {
                    ErrBox(hr, L"compiling resizer pixel shader X failed\n");
                }
                m_pUtilD3DBlob->Release();
                m_pUtilD3DBlob = nullptr;
                if ((m_u8ActiveResizer > 2) && !m_bNoHresize && !m_bNoVresize) {
                    if (FAILED(hr = m_fnD3DCompile(gk_aszResizerShader[m_u8ActiveResizer - 2], gk_au32LenResizerShader[m_u8ActiveResizer - 2], nullptr, m_aShaderMacros, nullptr, "mainV", m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &m_pUtilD3DBlob, nullptr))
                            || FAILED(hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(m_pUtilD3DBlob->GetBufferPointer()), &m_pResizerPixelShaderY))) {
                        ErrBox(hr, L"compiling resizer pixel shader Y failed\n");
                    }
                    m_pUtilD3DBlob->Release();
                    m_pUtilD3DBlob = nullptr;
                    if (FAILED(hr = m_pD3DDev->CreateTexture(m_dpPParam.BackBufferWidth, m_u32VideoHeight, 1, D3DUSAGE_RENDERTARGET, m_dfSurfaceType, D3DPOOL_DEFAULT, &m_pIntermediateResizeTexture, nullptr))) {
                        ErrBox(hr, L"creating intermediate resizer texture failed\n");
                    }
                    if (FAILED(hr = m_pIntermediateResizeTexture->GetSurfaceLevel(0, &m_pIntermediateResizeSurface))) {
                        ErrBox(hr, L"loading surface from intermediate resizer texture failed\n");
                    }
                    if (FAILED(hr = m_pD3DDev->ColorFill(m_pIntermediateResizeSurface, nullptr, mk_pRendererSettings->dwBackgoundColor))) {
                        ErrBox(hr, L"clearing intermediate resizer surface failed\n");
                    }
                }
            }
        }

        // final pass initialization
        if (m_dfSurfaceType != D3DFMT_X8R8G8B8) {
            // check if the final pass must be (re-)initialized
            if (!m_pFinalPixelShader || m_u8VMR9ColorManagementEnableCurrent != mk_pRendererSettings->iVMR9ColorManagementEnable || m_u8VMR9DitheringLevelsCurrent != mk_pRendererSettings->iVMR9DitheringLevels || m_bVMR9DisableInitialColorMixingCurrent != mk_pRendererSettings->iVMR9DisableInitialColorMixing
                    || m_u8VMR9ColorManagementAmbientLightCurrent != mk_pRendererSettings->iVMR9ColorManagementAmbientLight || m_u8VMR9ColorManagementIntentCurrent != mk_pRendererSettings->iVMR9ColorManagementIntent || m_u8VMR9ColorManagementWpAdaptStateCurrent != mk_pRendererSettings->iVMR9ColorManagementWpAdaptState || m_u32VMR9ColorManagementLookupQualityCurrent != mk_pRendererSettings->iVMR9ColorManagementLookupQuality || m_u8VMR9ColorManagementBPCCurrent != mk_pRendererSettings->iVMR9ColorManagementBPC
                    || m_u8VMR9DitheringTestEnableCurrent != mk_pRendererSettings->iVMR9DitheringTestEnable || m_u8VMR9ChromaFixCurrent != mk_pRendererSettings->iVMR9ChromaFix || m_u8ChromaTypeTest != m_u8ChromaType) {

                // Create the dither texture if necessary
                if ((mk_pRendererSettings->iVMR9DitheringLevels == 1 || mk_pRendererSettings->iVMR9DitheringLevels == 2) && !m_pDitherTexture) {
                    ASSERT(!m_pUtilTexture);
                    if (FAILED(hr = m_pD3DDev->CreateTexture(128, 128, 1, 0, D3DFMT_R32F, D3DPOOL_SYSTEMMEM, &m_pUtilTexture, nullptr))) {
                        ErrBox(hr, L"ditherer failed to create the temporary dither texture\n");
                    }
                    D3DLOCKED_RECT lockedRect;
                    if (FAILED(hr = m_pUtilTexture->LockRect(0, &lockedRect, nullptr, 0))) {
                        ErrBox(hr, L"ditherer failed to lock the temporary dither texture\n");
                    }
                    void* pDst = lockedRect.pBits;
                    ASSERT(!(reinterpret_cast<uintptr_t>(pDst) & 31));// 32-byte alignment guarantee
                    // copy the dither map, apply quantization
                    static __declspec(align(16)) float const afQ8bit[4] = {1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f};
                    static __declspec(align(16)) float const afQ10bit[4] = {1.0f / 1023.0f, 1.0f / 1023.0f, 1.0f / 1023.0f, 1.0f / 1023.0f};
                    float const* pQuant = (m_dpPParam.BackBufferFormat == D3DFMT_A2R10G10B10) ? afQ10bit : afQ8bit;
                    if (CPUGetEnabledExtensions() & CPUF_SUPPORTS_AVX) {// AVX code path
                        MoveDitherMatrixAVX(pDst, pQuant);// Assembly function
                    } else {// SSE code path
                        MoveDitherMatrix(pDst, pQuant);// Assembly function, can be removed once the compiler decently assembles the next procedure in intrinisic code and x64-specific code is added
                    }
                    /*
                    __m128 x0 = _mm_load_ps(pQuant), x1, x2, x3, x4, x5, x6, x7;
                    uintptr_t pSrc = reinterpret_cast<uintptr_t>(gk_aDitherMatrix);
                    unsigned __int32 j = 585;
                    goto SkipFirstInDitherMatrixLoop;
                    do {// handles 4095 (585*7) out the total of 4096 vectors of 4 singles
                        pSrc += 112;
                        pDst += 112;
                    SkipFirstInDitherMatrixLoop:
                        x1 = x0;
                        x2 = x0;
                        x3 = x0;
                        x4 = x0;
                        x5 = x0;
                        x6 = x0;
                        x7 = x0;
                        x1 = _mm_mul_ps(x1, *reinterpret_cast<__m128*>(pSrc));// trying to make the compiler do mulps xmm m128
                        x2 = _mm_mul_ps(x2, *reinterpret_cast<__m128*>(pSrc+16));
                        x3 = _mm_mul_ps(x3, *reinterpret_cast<__m128*>(pSrc+32));
                        x4 = _mm_mul_ps(x4, *reinterpret_cast<__m128*>(pSrc+48));
                        x5 = _mm_mul_ps(x5, *reinterpret_cast<__m128*>(pSrc+64));
                        x6 = _mm_mul_ps(x6, *reinterpret_cast<__m128*>(pSrc+80));
                        x7 = _mm_mul_ps(x7, *reinterpret_cast<__m128*>(pSrc+96));
                        _mm_store_ps(reinterpret_cast<float*>(pDst), x1);
                        _mm_store_ps(reinterpret_cast<float*>(pDst+16), x2);
                        _mm_store_ps(reinterpret_cast<float*>(pDst+32), x3);
                        _mm_store_ps(reinterpret_cast<float*>(pDst+48), x4);
                        _mm_store_ps(reinterpret_cast<float*>(pDst+64), x5);
                        _mm_store_ps(reinterpret_cast<float*>(pDst+80), x6);
                        _mm_store_ps(reinterpret_cast<float*>(pDst+96), x7);
                    } while(--j);
                    // multiply and store the last vector of 4 singles
                    x7 = _mm_mul_ps(x7, *reinterpret_cast<__m128*>(pSrc+112));
                    _mm_store_ps(reinterpret_cast<float*>(pDst+112), x7);
                    */
                    if (FAILED(hr = m_pUtilTexture->UnlockRect(0))) {
                        ErrBox(hr, L"ditherer failed to unlock the temporary dither texture\n");
                    }
                    ASSERT(!m_pDitherTexture);
                    if (FAILED(hr = m_pD3DDev->CreateTexture(128, 128, 1, 0, D3DFMT_R32F, D3DPOOL_DEFAULT, &m_pDitherTexture, nullptr))) {
                        ErrBox(hr, L"ditherer failed to create the final dither texture\n");
                    }
                    if (FAILED(hr = m_pD3DDev->UpdateTexture(m_pUtilTexture, m_pDitherTexture))) {
                        ErrBox(hr, L"ditherer failed to write the final dither texture\n");
                    }
                    m_pUtilTexture->Release();
                    m_pUtilTexture = nullptr;
                }

                // Initialize the color management if necessary
                if (mk_pRendererSettings->iVMR9ColorManagementEnable != 1) {
                    // set the internal color conversion to HD/sRGB if no color management is active
                    static __declspec(align(16)) char const kszXYZtoHDandsRGBmatrix[] = "3.5296034352,-1.0555622945,.0605843695,-1.6742990654,2.0430369477,-.2221426881,-.5430159131,.0452558574,1.1511030199";
                    __m128 x0 = _mm_load_ps(reinterpret_cast<float const*>(kszXYZtoHDandsRGBmatrix));
                    __m128 x1 = _mm_load_ps(reinterpret_cast<float const*>(kszXYZtoHDandsRGBmatrix + 16));
                    __m128 x2 = _mm_load_ps(reinterpret_cast<float const*>(kszXYZtoHDandsRGBmatrix + 32));
                    __m128 x3 = _mm_load_ps(reinterpret_cast<float const*>(kszXYZtoHDandsRGBmatrix + 48));
                    __m128 x4 = _mm_load_ps(reinterpret_cast<float const*>(kszXYZtoHDandsRGBmatrix + 64));
                    __m128 x5 = _mm_load_ps(reinterpret_cast<float const*>(kszXYZtoHDandsRGBmatrix + 80));
                    __m128 x6 = _mm_load_ps(reinterpret_cast<float const*>(kszXYZtoHDandsRGBmatrix + 96));
                    __m128 x7 = _mm_load_ps(reinterpret_cast<float const*>(kszXYZtoHDandsRGBmatrix + 112));
                    _mm_store_ps(reinterpret_cast<float*>(m_szXYZtoRGBmatrix), x0);
                    _mm_store_ps(reinterpret_cast<float*>(m_szXYZtoRGBmatrix + 16), x1);
                    _mm_store_ps(reinterpret_cast<float*>(m_szXYZtoRGBmatrix + 32), x2);
                    _mm_store_ps(reinterpret_cast<float*>(m_szXYZtoRGBmatrix + 48), x3);
                    _mm_store_ps(reinterpret_cast<float*>(m_szXYZtoRGBmatrix + 64), x4);
                    _mm_store_ps(reinterpret_cast<float*>(m_szXYZtoRGBmatrix + 80), x5);
                    _mm_store_ps(reinterpret_cast<float*>(m_szXYZtoRGBmatrix + 96), x6);
                    _mm_store_ps(reinterpret_cast<float*>(m_szXYZtoRGBmatrix + 112), x7);
                    // clear the Lut3D texture when it's no longer needed
                    if (m_pLut3DTexture) {
                        m_pLut3DTexture->Release();
                        m_pLut3DTexture = nullptr;
                    }
                } else if (!m_pLut3DTexture || m_u8VMR9ColorManagementAmbientLightCurrent != mk_pRendererSettings->iVMR9ColorManagementAmbientLight || m_u8VMR9ColorManagementIntentCurrent != mk_pRendererSettings->iVMR9ColorManagementIntent || m_u8VMR9ColorManagementWpAdaptStateCurrent != mk_pRendererSettings->iVMR9ColorManagementWpAdaptState || m_u32VMR9ColorManagementLookupQualityCurrent != mk_pRendererSettings->iVMR9ColorManagementLookupQuality || m_u8VMR9ColorManagementBPCCurrent != mk_pRendererSettings->iVMR9ColorManagementBPC) {
                    m_u8VMR9ColorManagementAmbientLightCurrent = mk_pRendererSettings->iVMR9ColorManagementAmbientLight;
                    m_u8VMR9ColorManagementIntentCurrent = mk_pRendererSettings->iVMR9ColorManagementIntent;
                    m_u8VMR9ColorManagementWpAdaptStateCurrent = mk_pRendererSettings->iVMR9ColorManagementWpAdaptState;
                    m_u8VMR9ColorManagementBPCCurrent = mk_pRendererSettings->iVMR9ColorManagementBPC;
                    if (m_pLut3DTexture) {
                        m_pLut3DTexture->Release();
                        m_pLut3DTexture = nullptr;
                    }

                    // Get the ICC profile path
                    HDC hDC = GetDC(m_hVideoWnd);
                    if (!hDC) {
                        ErrBox(0, L"color management failed to resolve the active display device window\n");
                    }
                    DWORD icmProfilePathSize = 0;
                    GetICMProfileW(hDC, &icmProfilePathSize, nullptr);// will never return TRUE in this mode
                    if (!icmProfilePathSize) {
                        ReleaseDC(m_hVideoWnd, hDC);
                        ErrBox(0, L"color management failed to find the active display device profile (.ICM file)\n");
                    }
                    wchar_t* szIcmProfilePath = reinterpret_cast<wchar_t*>(malloc(icmProfilePathSize << 1));
                    if (!szIcmProfilePath) {
                        ReleaseDC(m_hVideoWnd, hDC);
                        ErrBox(0, L"color management failed to allocate memory for storing the active display device profile (.ICM file) path and name\n");
                    }
                    EXECUTE_ASSERT(GetICMProfileW(hDC, &icmProfilePathSize, szIcmProfilePath));
                    ReleaseDC(m_hVideoWnd, hDC);
                    HANDLE hIcmProfileFile = CreateFileW(szIcmProfilePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
                    free(szIcmProfilePath);
                    FILETIME lastwritetime;
                    static_assert(sizeof(FILETIME) == 8, "struct FILETIME or platform settings changed");
                    if (!GetFileTime(hIcmProfileFile, nullptr, nullptr, &lastwritetime)) {
                        CloseHandle(hIcmProfileFile);
                        ErrBox(0, L"color management failed to read the timestamp of the active display device profile (.ICM file)\n");
                    }

                    // note: this expression is used everywhere to assert that the buffer is more than big enough at all times: ASSERT(pwcBuff - pwcFileName < 32768 - 128);
                    wchar_t* pwcFileName = reinterpret_cast<wchar_t*>(malloc(2 * 32768));
                    if (!pwcFileName) {
                        CloseHandle(hIcmProfileFile);
                        ErrBox(0, L"color management failed to allocate memory required for resolving the executable path name\n");
                    }
                    wchar_t* pwcBuff;

                    // try to find .ini settings file
                    DWORD dwLength = ::GetModuleFileName(nullptr, pwcFileName, 32767);
                    if (dwLength >= 8) {// L"A:\x.exe" is the shortest valid name
                        __declspec(align(8)) wchar_t const kszIniSuffix[] = L"ini";
                        *reinterpret_cast<__int64 __unaligned*>(pwcFileName + dwLength - 3) = *reinterpret_cast<__int64 const*>(kszIniSuffix);// last character is set to 0
                        if (INVALID_FILE_ATTRIBUTES != GetFileAttributesW(pwcFileName)) {// file exists
                            // truncate string to only the path name
                            DWORD i = dwLength - 6;// the last five characters (L"x.ini") are skipped in this loop
                            while (pwcFileName[i] != L'\\') {
                                --i;
                            }
                            pwcBuff = pwcFileName + 1 + i;// place pointer right after the L'\\'
                            goto PathSetNextToINI;
                        }
                    }
                    // use common user profile application data
                    // note: this function doesn't need more buffer than MAX_PATH, due to the the operating system restrictions on base system folder lengths
                    if (FAILED(hr = SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, pwcFileName))) {// note: szPath will still be a completely valid buffer at this point
                        free(pwcFileName);
                        CloseHandle(hIcmProfileFile);
                        ErrBox(hr, L"color management failed to find the path for the common user profile application data\n");
                    }
                    pwcBuff = pwcFileName + wcslen(pwcFileName);// SHGetFolderPathW() doesn't specify output size, it is only limited to writing MAX_PATH + zero end

                    static wchar_t const kszSubfolderLabel[] = L"\\Media Player Classic\\";
                    memcpy(pwcBuff, kszSubfolderLabel, sizeof(kszSubfolderLabel));// do not chop off end 0, it's required for GetFileAttributesW()
                    pwcBuff += _countof(kszSubfolderLabel) - 1;// do not count end 0
                    ASSERT(pwcBuff - pwcFileName < 32768 - 128);

                    DWORD dwFolderAttributes = GetFileAttributesW(pwcFileName);
                    if (!(dwFolderAttributes & FILE_ATTRIBUTE_DIRECTORY)) {// rare, but possible
                        free(pwcFileName);
                        CloseHandle(hIcmProfileFile);
                        ErrBox(0, L"color management encountered an invalid file with the name of the the application storage directory in the common user profile application data\n");
                    }
                    if (dwFolderAttributes == INVALID_FILE_ATTRIBUTES) {// only try to create the application storage directory if it doesn't exist yet
                        if (!CreateDirectoryW(pwcFileName, nullptr)) {
                            free(pwcFileName);
                            CloseHandle(hIcmProfileFile);
                            ErrBox(0, L"color management failed to create the application storage directory in the common user profile application data\n");
                        }
                    }
PathSetNextToINI:
                    // previous string construction: L"lCMS,abl=%s,rit=%s,wpa=%s,lqt=%hu,bpc=%s,qfm=%s16,pts=%016I64x.LUT3D"
                    static wchar_t const kszLCMSLabel[] = L"lCMS,abl=";
                    memcpy(pwcBuff, kszLCMSLabel, sizeof(kszLCMSLabel) - 2);// chop off end 0
                    pwcBuff += _countof(kszLCMSLabel) - 1;
                    ASSERT(pwcBuff - pwcFileName < 32768 - 128);

                    if (mk_pRendererSettings->iVMR9ColorManagementAmbientLight == AMBIENT_LIGHT_DARK) {
                        static wchar_t const kszDarkLabel[] = L"dark,rit=";
                        memcpy(pwcBuff, kszDarkLabel, sizeof(kszDarkLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszDarkLabel) - 1;
                        ASSERT(pwcBuff - pwcFileName < 32768 - 128);
                    } else if (mk_pRendererSettings->iVMR9ColorManagementAmbientLight == AMBIENT_LIGHT_DIM) {
                        static wchar_t const kszDimLabel[] = L"dim,rit=";
                        memcpy(pwcBuff, kszDimLabel, sizeof(kszDimLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszDimLabel) - 1;
                        ASSERT(pwcBuff - pwcFileName < 32768 - 128);
                    } else if (mk_pRendererSettings->iVMR9ColorManagementAmbientLight == AMBIENT_LIGHT_OFFICE) {
                        static wchar_t const kszOfficeLabel[] = L"office,rit=";
                        memcpy(pwcBuff, kszOfficeLabel, sizeof(kszOfficeLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszOfficeLabel) - 1;
                        ASSERT(pwcBuff - pwcFileName < 32768 - 128);
                    } else {
                        ASSERT(mk_pRendererSettings->iVMR9ColorManagementAmbientLight == AMBIENT_LIGHT_BRIGHT);
                        static wchar_t const kszBrightLabel[] = L"bright,rit=";
                        memcpy(pwcBuff, kszBrightLabel, sizeof(kszBrightLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszBrightLabel) - 1;
                        ASSERT(pwcBuff - pwcFileName < 32768 - 128);
                    }

                    if (mk_pRendererSettings->iVMR9ColorManagementIntent == INTENT_RELATIVE_COLORIMETRIC) {
                        static wchar_t const kszRelLabel[] = L"rel,wpa=";
                        memcpy(pwcBuff, kszRelLabel, sizeof(kszRelLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszRelLabel) - 1;
                        ASSERT(pwcBuff - pwcFileName < 32768 - 128);
                    } else {
                        ASSERT(mk_pRendererSettings->iVMR9ColorManagementIntent == INTENT_ABSOLUTE_COLORIMETRIC);
                        static wchar_t const kszAbsLabel[] = L"abs,wpa=";
                        memcpy(pwcBuff, kszAbsLabel, sizeof(kszAbsLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszAbsLabel) - 1;
                        ASSERT(pwcBuff - pwcFileName < 32768 - 128);
                    }

                    if (mk_pRendererSettings->iVMR9ColorManagementWpAdaptState == WPADAPT_STATE_FULL) {
                        static wchar_t const kszFullLabel[] = L"full,lqt=";
                        memcpy(pwcBuff, kszFullLabel, sizeof(kszFullLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszFullLabel) - 1;
                        ASSERT(pwcBuff - pwcFileName < 32768 - 128);
                    } else if (mk_pRendererSettings->iVMR9ColorManagementWpAdaptState == WPADAPT_STATE_MEDIUM) {
                        static wchar_t const kszMedLabel[] = L"med,lqt=";
                        memcpy(pwcBuff, kszMedLabel, sizeof(kszMedLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszMedLabel) - 1;
                        ASSERT(pwcBuff - pwcFileName < 32768 - 128);
                    } else {
                        ASSERT(mk_pRendererSettings->iVMR9ColorManagementWpAdaptState == WPADAPT_STATE_NONE);
                        static wchar_t const kszNoneLabel[] = L"none,lqt=";
                        memcpy(pwcBuff, kszNoneLabel, sizeof(kszNoneLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszNoneLabel) - 1;
                        ASSERT(pwcBuff - pwcFileName < 32768 - 128);
                    }

                    unsigned __int32 u32LQT = mk_pRendererSettings->iVMR9ColorManagementLookupQuality;
                    ASSERT(u32LQT >= 64 && u32LQT <= 256);
                    if (u32LQT >= 100) {
                        wchar_t hundreds = L'1';
                        if (u32LQT >= 200) {
                            u32LQT -= 100;
                            hundreds = L'2';
                        }
                        u32LQT -= 100;
                        *pwcBuff = hundreds;
                        pwcBuff += 1;
                        ASSERT(pwcBuff - pwcFileName < 32768 - 128);
                    }
                    unsigned __int8 u8LQTs = static_cast<unsigned __int8>(u32LQT);// the hundreds are taken out, the largest possible number is 99
                    unsigned __int8 u8tens = u8LQTs / 10;
                    unsigned __int8 u8singles = u8LQTs % 10;
                    *pwcBuff = u8tens + '0';// zero-extended on the write to memory
                    pwcBuff += 1;
                    *pwcBuff = u8singles + '0';
                    pwcBuff += 1;
                    ASSERT(pwcBuff - pwcFileName < 32768 - 128);

                    if (mk_pRendererSettings->iVMR9ColorManagementBPC) {
                        static wchar_t const kszBPCYLabel[] = L",bpc=yes";
                        memcpy(pwcBuff, kszBPCYLabel, sizeof(kszBPCYLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszBPCYLabel) - 1;
                        ASSERT(pwcBuff - pwcFileName < 32768 - 128);
                    } else {
                        static wchar_t const kszBPCNLabel[] = L",bpc=no";
                        memcpy(pwcBuff, kszBPCNLabel, sizeof(kszBPCNLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszBPCNLabel) - 1;
                        ASSERT(pwcBuff - pwcFileName < 32768 - 128);
                    }

                    if (m_pRenderersData->m_bFP32Support) {
                        static wchar_t const kszQUI16Label[] = L",qfm=ui16,pts=";
                        memcpy(pwcBuff, kszQUI16Label, sizeof(kszQUI16Label) - 2);// chop off end 0
                        pwcBuff += _countof(kszQUI16Label) - 1;
                        ASSERT(pwcBuff - pwcFileName < 32768 - 128);
                    } else {
                        static wchar_t const kszQFP16Label[] = L",qfm=fp16,pts=";
                        memcpy(pwcBuff, kszQFP16Label, sizeof(kszQFP16Label) - 2);// chop off end 0
                        pwcBuff += _countof(kszQFP16Label) - 1;
                        ASSERT(pwcBuff - pwcFileName < 32768 - 128);
                    }

                    unsigned __int64 u64WTime = *reinterpret_cast<unsigned __int64*>(&lastwritetime);
                    // standard method for converting numbers to hex strings
                    unsigned __int8 u8Nibble = u64WTime >> 60;// each hexadecimal char stores 4 bits
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[0] = u8Nibble;// zero-extended on the write to memory
                    u8Nibble = (u64WTime >> 56) & 15;
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[1] = u8Nibble;
                    u8Nibble = (u64WTime >> 52) & 15;
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[2] = u8Nibble;
                    u8Nibble = (u64WTime >> 48) & 15;
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[3] = u8Nibble;
                    u8Nibble = (u64WTime >> 44) & 15;
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[4] = u8Nibble;
                    u8Nibble = (u64WTime >> 40) & 15;
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[5] = u8Nibble;
                    u8Nibble = (u64WTime >> 36) & 15;
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[6] = u8Nibble;
                    u8Nibble = (u64WTime >> 32) & 15;
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[7] = u8Nibble;
                    u8Nibble = (u64WTime >> 28) & 15;
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[8] = u8Nibble;
                    u8Nibble = (u64WTime >> 24) & 15;
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[9] = u8Nibble;
                    u8Nibble = (u64WTime >> 20) & 15;
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[10] = u8Nibble;
                    u8Nibble = (u64WTime >> 16) & 15;
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[11] = u8Nibble;
                    u8Nibble = (u64WTime >> 12) & 15;
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[12] = u8Nibble;
                    u8Nibble = (u64WTime >> 8) & 15;
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[13] = u8Nibble;
                    u8Nibble = (u64WTime >> 4) & 15;
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[14] = u8Nibble;
                    u8Nibble = u64WTime & 15;
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    pwcBuff[15] = u8Nibble;
                    pwcBuff += 16;

                    static wchar_t const kszLUT3DLabel[] = L".LUT3D";
                    memcpy(pwcBuff, kszLUT3DLabel, sizeof(kszLUT3DLabel));// include the end 0
                    ASSERT(pwcBuff + _countof(kszLUT3DLabel) - 1 - pwcFileName < 32768 - 128);

                    // Create the LUT3D texture
                    D3DFORMAT Lut3DCF = m_pRenderersData->m_bFP32Support ? D3DFMT_A16B16G16R16 : D3DFMT_A16B16G16R16F;
                    ASSERT(!m_pLut3DTexture && !m_pUtil3DTexture);
                    if (FAILED(hr = m_pD3DDev->CreateVolumeTexture(mk_pRendererSettings->iVMR9ColorManagementLookupQuality, mk_pRendererSettings->iVMR9ColorManagementLookupQuality, mk_pRendererSettings->iVMR9ColorManagementLookupQuality, 1, 0, Lut3DCF, D3DPOOL_DEFAULT, &m_pLut3DTexture, nullptr))) {
                        free(pwcFileName);
                        CloseHandle(hIcmProfileFile);
                        ErrBox(hr, L"color management failed to create the final LUT3D texture\n");
                    }
                    if (FAILED(hr = m_pD3DDev->CreateVolumeTexture(mk_pRendererSettings->iVMR9ColorManagementLookupQuality, mk_pRendererSettings->iVMR9ColorManagementLookupQuality, mk_pRendererSettings->iVMR9ColorManagementLookupQuality, 1, 0, Lut3DCF, D3DPOOL_SYSTEMMEM, &m_pUtil3DTexture, nullptr))) {
                        free(pwcFileName);
                        CloseHandle(hIcmProfileFile);
                        ErrBox(hr, L"color management failed to create the temporary LUT3D texture\n");
                    }
                    D3DLOCKED_BOX lockedBox;
                    if (FAILED(hr = m_pUtil3DTexture->LockBox(0, &lockedBox, nullptr, 0))) {
                        free(pwcFileName);
                        CloseHandle(hIcmProfileFile);
                        ErrBox(hr, L"color management failed to lock the temporary LUT3D texture\n");
                    }

                    // Open the output profile
                    int ihIcmProfileFile = _open_osfhandle(reinterpret_cast<intptr_t>(hIcmProfileFile), _O_RDONLY | _O_BINARY);// CloseHandle() is no longer required after this call, fclose() takes care of that
                    ASSERT(ihIcmProfileFile != -1);
                    FILE* outputProfileStream = _wfdopen(ihIcmProfileFile, L"rb");
                    ASSERT(outputProfileStream);
                    cmsHPROFILE hOutputProfile = cmsOpenProfileFromStream(outputProfileStream, "r");
                    if (!hOutputProfile) {
                        free(pwcFileName);
                        fclose(outputProfileStream);
                        ErrBox(0, L"color management failed to create an output color profile from the active display device profile (.ICM file)\n");
                    }

                    // notes about handling the input white point in the initial pass pixel shaders:
                    // the intention is to scale the input white value linearly down, so that it is a color that exists in the output color space
                    // we currently don't support a different adaptation value for compensating the input-to-output white point other than the truncated D65 white point here
                    // when imput formats that have a different white point get mixer support, add code here and to the mixer to handle those
                    // the next part uses the input white point at "calculate scaling factor" and for the second XYZ transform

                    cmsBool BPC[2] = {false, false};// not const; this array is written in the process, but only with 'false' again, it is also re-used for creating the next XYZ to RGB transform
                    double adRGBtoXYZmatrix[9];
                    {
                        cmsHPROFILE hPlainXYZProfile = cmsCreateXYZProfile();
                        if (!hPlainXYZProfile) {
                            free(pwcFileName);
                            cmsCloseProfile(hOutputProfile);
                            fclose(outputProfileStream);
                            ErrBox(0, L"color management failed to create a plain XYZ profile\n");
                        }

                        // create the absolute XYZ to RGB matrix string for the initial pixel shader pass
                        // we don't use an input white point-adapted XYZ profile for this part, unlike the next XYZ profile
                        // the XYZ profile here gets the default D50 white point, like most ICC/ICM profiles, so it is suitable here
                        // for the rest the white point for the XYZ profile gets ignored because of the absolute colorimetric intent with no black point compensation and no white point adaptation
                        cmsHPROFILE hProfilesRGBtoXYZ[2] = {hOutputProfile, hPlainXYZProfile};
                        static cmsUInt32Number const absoluteintents[2] = {INTENT_ABSOLUTE_COLORIMETRIC, INTENT_ABSOLUTE_COLORIMETRIC};
                        static cmsFloat64Number const noWPAdaptStates[2] = {0.0, 0.0};
                        // const_cast is indeed rare, but it's only to make these arrays ROMmable, they're never written
                        cmsHTRANSFORM hTransformRGBtoXYZ = cmsCreateExtendedTransform(nullptr, 2, hProfilesRGBtoXYZ, BPC, const_cast<cmsUInt32Number*>(absoluteintents), const_cast<cmsFloat64Number*>(noWPAdaptStates), nullptr, 0, TYPE_RGB_DBL, TYPE_XYZ_DBL, cmsFLAGS_HIGHRESPRECALC | cmsFLAGS_NOOPTIMIZE);// cmsFLAGS_NOOPTIMIZE is recommended for transforms that are used only a few times
                        cmsCloseProfile(hPlainXYZProfile);

                        if (!hTransformRGBtoXYZ) {
                            free(pwcFileName);
                            cmsCloseProfile(hOutputProfile);
                            fclose(outputProfileStream);
                            ErrBox(0, L"color management failed to create the RGB to plain XYZ color transform\n");
                        }

                        static double const kadRGBWpoints[12] = {
                            1.0, 0.0, 0.0,
                            0.0, 1.0, 0.0,
                            0.0, 0.0, 1.0,
                            1.0, 1.0, 1.0
                        };
                        double adRGBWpointsOut[12];
                        cmsDoTransform(hTransformRGBtoXYZ, kadRGBWpoints, adRGBWpointsOut, 4);
                        cmsDeleteTransform(hTransformRGBtoXYZ);

                        // normalize to Y = 1 for each
                        double Yrr = 1.0 / adRGBWpointsOut[1];
                        double Xr = adRGBWpointsOut[0] * Yrr;
                        double Zr = adRGBWpointsOut[2] * Yrr;
                        double Ygr = 1.0 / adRGBWpointsOut[4];
                        double Xg = adRGBWpointsOut[3] * Ygr;
                        double Zg = adRGBWpointsOut[5] * Ygr;
                        double Ybr = 1.0 / adRGBWpointsOut[7];
                        double Xb = adRGBWpointsOut[6] * Ybr;
                        double Zb = adRGBWpointsOut[8] * Ybr;
                        double Ywr = 1.0 / adRGBWpointsOut[10];
                        double Xw = adRGBWpointsOut[9] * Ywr;
                        double Zw = adRGBWpointsOut[11] * Ywr;
                        double maxXZw = (Xw > Zw) ? Xw : Zw;

                        // XYZ to xyY color space: xyY = {X / (X + Y + Z), Y / (X + Y + Z), Y}
                        // xyY to XYZ  color space: XYZ = {Y * x / y, Y, Y * (1.0 - x - y) / y}
                        double xyYyw = 1.0 / (1.0 + Xw + Zw);
                        double xyYxw = Xw * xyYyw;
                        // calculate scaling factor
                        // this is to tone down the input white point relative to the output white point
                        // the vector length is calculated in xyY color space, as doing XYZ to XYZ mapping would mean calculating including intensity-dependent parts, instead of only tint-dependent parts
                        // the scaling factor is only an approximation based on the input and output white points, but it's a very reasonable way to prevent clipping in the highest whites of the input medium
                        double xwDiff = xyYxw - kxyYwpForNTSCPALSECAMandHD.x;
                        double ywDiff = xyYyw - kxyYwpForNTSCPALSECAMandHD.y;
                        double vLength = sqrt(xwDiff * xwDiff + ywDiff * ywDiff);// the vector length, a Pythagorean term
                        double ScaleMatrix = 1.0 + vLength;
                        ASSERT(ScaleMatrix < 1.05 && ScaleMatrix >= 1.0);// it will generally be less than a 5% difference

                        // reciprocal of the matrix determinant
                        double MdetR = 1.0 / (Xr * (Zb - Zg) - Xg * (Zb - Zr) + Xb * (Zg - Zr));
                        // body of the matrix inverse
                        double A = Zb - Zg, D = Xb * Zg - Xg * Zb, G = Xg - Xb,
                               B = Zr - Zb, E = Xr * Zb - Xb * Zr, H = Xb - Xr,
                               C = Zg - Zr, F = Xg * Zr - Xr * Zg, K = Xr - Xg;

                        double Sr = (Xw * A + D + Zw * G) * MdetR;
                        double Sg = (Xw * B + E + Zw * H) * MdetR;
                        double Sb = (Xw * C + F + Zw * K) * MdetR;
                        // finalize RGB to XYZ matrix
                        double MXr = Sr * Xr, MXg = Sg * Xg, MXb = Sb * Xb,
                               MYr = Sr,      MYg = Sg,      MYb = Sb,
                               MZr = Sr * Zr, MZg = Sg * Zg, MZb = Sb * Zb;

                        // normalize the matrix if X or Z is larger than 1 (the normalized Y)
                        if (maxXZw > 1.0) {
                            double mn = 1.0 / maxXZw;
                            MXr *= mn, MXg *= mn, MXb *= mn;
                            MYr *= mn, MYg *= mn, MYb *= mn;
                            MZr *= mn, MZg *= mn, MZb *= mn;
                        }
                        // store the matrix for the next transform
                        adRGBtoXYZmatrix[0] = MXr, adRGBtoXYZmatrix[1] = MXg, adRGBtoXYZmatrix[2] = MXb;
                        adRGBtoXYZmatrix[3] = MYr, adRGBtoXYZmatrix[4] = MYg, adRGBtoXYZmatrix[5] = MYb;
                        adRGBtoXYZmatrix[6] = MZr, adRGBtoXYZmatrix[7] = MZg, adRGBtoXYZmatrix[8] = MZb;

                        // this part will expand the values by the scaling factor
                        // this generally means larger values in the RGB to XYZ matrix (unused) and smaller values in the XYZ to RGB matrix, exactly what we want
                        MXr *= ScaleMatrix, MXg *= ScaleMatrix, MXb *= ScaleMatrix;
                        MYr *= ScaleMatrix, MYg *= ScaleMatrix, MYb *= ScaleMatrix;
                        MZr *= ScaleMatrix, MZg *= ScaleMatrix, MZb *= ScaleMatrix;

                        // XYZ to RGB, just invert the new matrix
                        // reciprocal of the matrix determinant
                        double WdetR = 1.0 / (MXr * (MYg * MZb - MYb * MZg) - MXg * (MZb * MYr - MYb * MZr) + MXb * (MYr * MZg - MYg * MZr));
                        // body of the matrix inverse
                        double WRx = (MYg * MZb - MYb * MZg) * WdetR, WRy = (MXb * MZg - MXg * MZb) * WdetR, WRz = (MXg * MYb - MXb * MYg) * WdetR,
                               WGx = (MYb * MZr - MYr * MZb) * WdetR, WGy = (MXr * MZb - MXb * MZr) * WdetR, WGz = (MXb * MYr - MXr * MYb) * WdetR,
                               WBx = (MYr * MZg - MYg * MZr) * WdetR, WBy = (MXg * MZr - MXr * MZg) * WdetR, WBz = (MXr * MYg - MXg * MYr) * WdetR;

                        // pixel shaders are column major by default:
                        int iLenXYZtoRGBmatrix = _snprintf_s(m_szXYZtoRGBmatrix, 128, _TRUNCATE, "%.10f,%.10f,%.10f,%.10f,%.10f,%.10f,%.10f,%.10f,%.10f", WRx, WGx, WBx, WRy, WGy, WBy, WRz, WGz, WBz);
                        ASSERT(iLenXYZtoRGBmatrix != -1);
                    }

                    unsigned __int32 u32Lut3DVoxelCount = mk_pRendererSettings->iVMR9ColorManagementLookupQuality;
                    u32Lut3DVoxelCount = u32Lut3DVoxelCount * u32Lut3DVoxelCount * u32Lut3DVoxelCount;// cube
                    unsigned __int32 u32Lut3DByteSize = u32Lut3DVoxelCount << 3;
                    HANDLE hRead = CreateFileW(pwcFileName, FILE_READ_DATA, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
                    if (hRead != INVALID_HANDLE_VALUE) {
                        // Cleanup
                        free(pwcFileName);
                        cmsCloseProfile(hOutputProfile);
                        fclose(outputProfileStream);

                        DWORD dwNumberOfBytesRead;
                        BOOL boSucces = ReadFile(hRead, lockedBox.pBits, u32Lut3DByteSize, &dwNumberOfBytesRead, nullptr);
                        ASSERT(dwNumberOfBytesRead == u32Lut3DByteSize);
                        CloseHandle(hRead);
                        if (!boSucces) {
                            ErrBox(0, L"color management failed to read a LUT3D file\n");
                        }
                    } else {
                        // Create an XYZ profile with the input white point as reference
                        // for the absolute colorimetric intent with no black point compensation and no white point adaptation that white point doesn't matter (see previous text for the plain XYZ profile)
                        static cmsFloat64Number const kdOne = 1.0;
                        cmsToneCurve* transferFunction = cmsBuildParametricToneCurve(0, 1, &kdOne);
                        cmsToneCurve* transferFunctionRGB[3] = {transferFunction, transferFunction, transferFunction};
                        cmsHPROFILE hXYZProfile = cmsCreateRGBProfile(&kxyYwpForNTSCPALSECAMandHD, &kTxyYidentity, transferFunctionRGB);
                        cmsFreeToneCurve(transferFunction);// Cleanup
                        if (!hXYZProfile) {
                            free(pwcFileName);
                            cmsCloseProfile(hOutputProfile);
                            fclose(outputProfileStream);
                            ErrBox(0, L"color management failed to create a custom white-point XYZ profile\n");
                        }

#if WPADAPT_STATE_FULL != 2 || WPADAPT_STATE_MEDIUM != 1 || WPADAPT_STATE_NONE != 0
#error renderer settings defines changed, edit this section of the code to correct this issue
#endif
                        // Create the transforms using the user settings
                        cmsUInt32Number intents[2];// the intents are compile-time asserted at the top of this file
                        cmsFloat64Number WPAdaptStates[2];
                        intents[0] = intents[1] = mk_pRendererSettings->iVMR9ColorManagementIntent;
                        WPAdaptStates[0] = WPAdaptStates[1] = 0.5 * static_cast<cmsFloat64Number>(mk_pRendererSettings->iVMR9ColorManagementWpAdaptState);
                        BPC[0] = BPC[1] = mk_pRendererSettings->iVMR9ColorManagementBPC;

                        cmsHPROFILE hProfilesXYZtoRGB[2] = {hXYZProfile, hOutputProfile};
                        cmsHTRANSFORM hTransformXYZtoRGB = cmsCreateExtendedTransform(nullptr, 2, hProfilesXYZtoRGB, BPC, intents, WPAdaptStates, nullptr, 0, TYPE_RGB_DBL, m_pRenderersData->m_bFP32Support ? TYPE_RGBA_16 : TYPE_RGBA_HALF_FLT, cmsFLAGS_HIGHRESPRECALC);
                        // Cleanup
                        cmsCloseProfile(hOutputProfile);
                        cmsCloseProfile(hXYZProfile);
                        fclose(outputProfileStream);

                        if (!hTransformXYZtoRGB) {
                            free(pwcFileName);
                            ErrBox(0, L"color management failed to create a color transform from the active display device profile (.ICM file)\n");
                        }

                        // Set up initial CIECAM02 parameters
                        cmsHANDLE hCIECAM02F = cmsCIECAM02Init(nullptr, &ViewingConditionsStudio);
                        if (!hCIECAM02F) {
                            free(pwcFileName);
                            cmsDeleteTransform(hTransformXYZtoRGB);
                            ErrBox(0, L"color management failed to create the forward CIECAM02 color transform\n");
                        }

                        // Get the ambient viewing condition parameters
#if AMBIENT_LIGHT_DARK != 3 || AMBIENT_LIGHT_DIM != 2 || AMBIENT_LIGHT_OFFICE != 1 || AMBIENT_LIGHT_BRIGHT != 0
#error renderer settings defines changed, edit this section of the code to correct this issue
#endif
                        cmsViewingConditions const* pUservc = akViewingConditions + mk_pRendererSettings->iVMR9ColorManagementAmbientLight;

                        cmsHANDLE hCIECAM02R = cmsCIECAM02Init(nullptr, pUservc);
                        if (!hCIECAM02R) {
                            free(pwcFileName);
                            cmsDeleteTransform(hTransformXYZtoRGB);
                            cmsCIECAM02Done(hCIECAM02F);
                            ErrBox(0, L"color management failed to create the reverse CIECAM02 color transform\n");
                        }

                        // create custom RGB to XYZ lookups for the worker threads
                        double* pRGBtoXYZlookups = reinterpret_cast<double*>(malloc(8 * 9 * 256));// for smaller tables than the maximum of 256, this is just padded
                        if (!pRGBtoXYZlookups) {
                            free(pwcFileName);
                            cmsDeleteTransform(hTransformXYZtoRGB);
                            cmsCIECAM02Done(hCIECAM02F);
                            cmsCIECAM02Done(hCIECAM02R);
                            ErrBox(0, L"color management failed to allocate memory for the temporary intermediate lookup table");
                        }
                        unsigned __int32 u32LookupQuality = mk_pRendererSettings->iVMR9ColorManagementLookupQuality;
                        double dLut3DSizedr = 1.0 / (static_cast<double>(static_cast<__int32>(u32LookupQuality)) - 1.0);// the standard converter only does a proper job with signed values
                        // there's no point in transforming the first set
                        pRGBtoXYZlookups[0] = pRGBtoXYZlookups[1] = pRGBtoXYZlookups[2] = 0.0;
                        pRGBtoXYZlookups[768] = pRGBtoXYZlookups[769] = pRGBtoXYZlookups[770] = 0.0;
                        pRGBtoXYZlookups[1536] = pRGBtoXYZlookups[1537] = pRGBtoXYZlookups[1538] = 0.0;

                        // fill the lookup
                        // the order is column vector order because the worker threads use three doubles per iteration of red, green and blue, so it's better to keep these three grouped together for better cacheability
                        double* pMc = pRGBtoXYZlookups + 3;
                        double dC = dLut3DSizedr;
                        unsigned __int8 i = static_cast<unsigned __int8>(u32LookupQuality - 1);// u32LookupQuality is 256 or less
                        do {
                            double d = dC * dC * dC;
                            pMc[0] = d * adRGBtoXYZmatrix[0], pMc[1] = d * adRGBtoXYZmatrix[3], pMc[2] = d * adRGBtoXYZmatrix[6];
                            pMc[768] = d * adRGBtoXYZmatrix[1], pMc[769] = d * adRGBtoXYZmatrix[4], pMc[770] = d * adRGBtoXYZmatrix[7];
                            pMc[1536] = d * adRGBtoXYZmatrix[2], pMc[1537] = d * adRGBtoXYZmatrix[5], pMc[1538] = d * adRGBtoXYZmatrix[8];
                            pMc += 3;
                            dC += dLut3DSizedr;
                        } while (--i);

                        // multi-threading main function
                        COLORTRANSFORM Transforms[16];
                        COLORTRANSFORM* ptf = Transforms;
                        void* pOutput = lockedBox.pBits;
                        unsigned __int32 u32OneSixteenthO = u32Lut3DVoxelCount >> 1;// * 8 (bytes per output) / 16 (threads)
                        unsigned __int32 u32BlueSlicePart = (u32LookupQuality * 3) >> 4;// workload per thread is split by dividing the blue entries, this number is multiplied by 3 because it's used to offset the pointer to the blue array in the RGB to XYZ lookup table
                        unsigned __int32 u32BlueSliceCounter = 0;
                        i = 16;
                        do {
                            ptf->pRGBtoXYZlookups = pRGBtoXYZlookups;
                            ptf->hCIECAM02F = hCIECAM02F;
                            ptf->hCIECAM02R = hCIECAM02R;
                            ptf->hTransformXYZtoRGB = hTransformXYZtoRGB;
                            ptf->pOutput = pOutput;
                            ptf->u32LookupQuality = u32LookupQuality;
                            ptf->u32BlueSliceStart = u32BlueSliceCounter;
                            ++ptf;
                            reinterpret_cast<uintptr_t&>(pOutput) += u32OneSixteenthO;
                            u32BlueSliceCounter += u32BlueSlicePart;
                        } while (--i);

                        HANDLE hProcesses[15];
                        hProcesses[0] = ::CreateThread(nullptr, 0x10000, ColorMThreadStatic, Transforms + 1, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                        if (!hProcesses[0]) {
                            goto ColorMCreateThreadFailed;
                        }
                        hProcesses[1] = ::CreateThread(nullptr, 0x10000, ColorMThreadStatic, Transforms + 2, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                        if (!hProcesses[1]) {
                            goto ColorMCreateThreadFailed;
                        }
                        hProcesses[2] = ::CreateThread(nullptr, 0x10000, ColorMThreadStatic, Transforms + 3, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                        if (!hProcesses[2]) {
                            goto ColorMCreateThreadFailed;
                        }
                        hProcesses[3] = ::CreateThread(nullptr, 0x10000, ColorMThreadStatic, Transforms + 4, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                        if (!hProcesses[3]) {
                            goto ColorMCreateThreadFailed;
                        }
                        hProcesses[4] = ::CreateThread(nullptr, 0x10000, ColorMThreadStatic, Transforms + 5, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                        if (!hProcesses[4]) {
                            goto ColorMCreateThreadFailed;
                        }
                        hProcesses[5] = ::CreateThread(nullptr, 0x10000, ColorMThreadStatic, Transforms + 6, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                        if (!hProcesses[5]) {
                            goto ColorMCreateThreadFailed;
                        }
                        hProcesses[6] = ::CreateThread(nullptr, 0x10000, ColorMThreadStatic, Transforms + 7, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                        if (!hProcesses[6]) {
                            goto ColorMCreateThreadFailed;
                        }
                        hProcesses[7] = ::CreateThread(nullptr, 0x10000, ColorMThreadStatic, Transforms + 8, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                        if (!hProcesses[7]) {
                            goto ColorMCreateThreadFailed;
                        }
                        hProcesses[8] = ::CreateThread(nullptr, 0x10000, ColorMThreadStatic, Transforms + 9, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                        if (!hProcesses[8]) {
                            goto ColorMCreateThreadFailed;
                        }
                        hProcesses[9] = ::CreateThread(nullptr, 0x10000, ColorMThreadStatic, Transforms + 10, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                        if (!hProcesses[9]) {
                            goto ColorMCreateThreadFailed;
                        }
                        hProcesses[10] = ::CreateThread(nullptr, 0x10000, ColorMThreadStatic, Transforms + 11, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                        if (!hProcesses[10]) {
                            goto ColorMCreateThreadFailed;
                        }
                        hProcesses[11] = ::CreateThread(nullptr, 0x10000, ColorMThreadStatic, Transforms + 12, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                        if (!hProcesses[11]) {
                            goto ColorMCreateThreadFailed;
                        }
                        hProcesses[12] = ::CreateThread(nullptr, 0x10000, ColorMThreadStatic, Transforms + 13, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                        if (!hProcesses[12]) {
                            goto ColorMCreateThreadFailed;
                        }
                        hProcesses[13] = ::CreateThread(nullptr, 0x10000, ColorMThreadStatic, Transforms + 14, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                        if (!hProcesses[13]) {
                            goto ColorMCreateThreadFailed;
                        }
                        hProcesses[14] = ::CreateThread(nullptr, 0x10000, ColorMThreadStatic, Transforms + 15, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                        if (!hProcesses[14]) {
                            goto ColorMCreateThreadFailed;
                        }
                        goto ColorMCreateThreadSucceeded;
ColorMCreateThreadFailed:
                        free(pwcFileName);
                        i = 0;
                        do {
                            if (!hProcesses[i]) {
                                break;
                            }
                            TerminateThread(hProcesses[i], 0xDEAD);
                            CloseHandle(hProcesses[i]);
                            ++i;
                        } while (i < 15);
                        free(pwcFileName);
                        free(pRGBtoXYZlookups);
                        cmsDeleteTransform(hTransformXYZtoRGB);
                        cmsCIECAM02Done(hCIECAM02F);
                        cmsCIECAM02Done(hCIECAM02R);
                        ErrBox(0, L"color management failed to create a worker thread");

ColorMCreateThreadSucceeded:
                        // process one block in this thread
                        ColorMThreadStatic(Transforms);

                        if (WaitForMultipleObjects(15, hProcesses, TRUE, 480000) == WAIT_TIMEOUT) {// 8 minutes should be much more than enough
                            ASSERT(0);
                            free(pwcFileName);
                            ptrdiff_t j = 14;
                            do {
                                TerminateThread(hProcesses[j], 0xDEAD);
                                CloseHandle(hProcesses[j]);
                            } while (--j >= 0);
                            free(pwcFileName);
                            free(pRGBtoXYZlookups);
                            cmsDeleteTransform(hTransformXYZtoRGB);
                            cmsCIECAM02Done(hCIECAM02F);
                            cmsCIECAM02Done(hCIECAM02R);
                            ErrBox(0, L"color management failed to complete a worker thread task within 8 minutes");
                        }
                        ptrdiff_t j = 14;
                        do {
                            EXECUTE_ASSERT(CloseHandle(hProcesses[j]));
                        } while (--j >= 0);

                        // Cleanup
                        free(pRGBtoXYZlookups);
                        cmsDeleteTransform(hTransformXYZtoRGB);
                        cmsCIECAM02Done(hCIECAM02F);
                        cmsCIECAM02Done(hCIECAM02R);

                        // Finish the texture creation, create the output file
                        // the alignment requirement for FILE_FLAG_NO_BUFFERING is not a problem here, the texture base is at least cache line aligned by its allocator
                        HANDLE hWrite = CreateFileW(pwcFileName, FILE_WRITE_DATA, 0, nullptr, CREATE_NEW, FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
                        free(pwcFileName);
                        if (hWrite == INVALID_HANDLE_VALUE) {
                            ErrBox(0, L"color management failed to create a new, empty LUT3D file\n");
                        }
                        DWORD dwNumberOfBytesWritten;
                        BOOL bSucces = WriteFile(hWrite, lockedBox.pBits, u32Lut3DByteSize, &dwNumberOfBytesWritten, nullptr);
                        ASSERT(dwNumberOfBytesWritten == u32Lut3DByteSize);
                        EXECUTE_ASSERT(CloseHandle(hWrite));
                        if (!bSucces) {
                            ErrBox(0, L"color management failed to write to a new LUT3D file\n");
                        }
                    }

                    if (FAILED(hr = m_pUtil3DTexture->UnlockBox(0))) {
                        ErrBox(hr, L"color management failed to unlock the temporary LUT3D texture\n");
                    }
                    if (FAILED(hr = m_pD3DDev->UpdateTexture(m_pUtil3DTexture, m_pLut3DTexture))) {
                        ErrBox(hr, L"color management failed to write the final LUT3D texture\n");
                    }
                    m_pUtil3DTexture->Release();
                    m_pUtil3DTexture = nullptr;
                }

                // Compile the final pass pixel shader if necessary
                if (!m_pFinalPixelShader || m_u8VMR9ColorManagementEnableCurrent != mk_pRendererSettings->iVMR9ColorManagementEnable || m_u32VMR9ColorManagementLookupQualityCurrent != mk_pRendererSettings->iVMR9ColorManagementLookupQuality || m_u8VMR9DitheringLevelsCurrent != mk_pRendererSettings->iVMR9DitheringLevels || m_u8VMR9DitheringTestEnableCurrent != mk_pRendererSettings->iVMR9DitheringTestEnable) {
                    m_u8VMR9ColorManagementEnableCurrent = mk_pRendererSettings->iVMR9ColorManagementEnable;
                    m_u32VMR9ColorManagementLookupQualityCurrent = mk_pRendererSettings->iVMR9ColorManagementLookupQuality;
                    m_u8VMR9DitheringLevelsCurrent = mk_pRendererSettings->iVMR9DitheringLevels;
                    m_u8VMR9DitheringTestEnableCurrent = mk_pRendererSettings->iVMR9DitheringTestEnable;
                    if (m_pFinalPixelShader) {
                        m_pFinalPixelShader->Release();
                        m_pFinalPixelShader = nullptr;
                    }
                    m_u8VMR9FrameInterpolationCurrent <<= 1;// to pass the next check list at the frame interpolation section (doesn't affect 0)

                    unsigned __int8 u8Nibble;
                    // m_szLut3DSize; standard method for converting numbers to hex strings
                    ASSERT(mk_pRendererSettings->iVMR9ColorManagementLookupQuality <= 256);// the method implementation limit here
                    if (mk_pRendererSettings->iVMR9ColorManagementLookupQuality == 256) {
                        *reinterpret_cast<__int32*>(m_szLut3DSize) = '652';// decimal, but okay here, byte 4 is also correctly set to 0
                    } else {
                        *reinterpret_cast<__int16*>(m_szLut3DSize) = 'x0';
                        u8Nibble = static_cast<unsigned __int8>(mk_pRendererSettings->iVMR9ColorManagementLookupQuality >> 4);// each hexadecimal char stores 4 bits
                        u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                        m_szLut3DSize[2] = u8Nibble;
                        ASSERT(!(mk_pRendererSettings->iVMR9ColorManagementLookupQuality & 15));// lowest 4 bits, all implementation options should be modulo 16, so that these bits are 0
                        m_szLut3DSize[3] = '0';
                        m_szLut3DSize[4] = 0;
                    }

                    // m_szDitheringLevel; standard method for converting numbers to hex strings
                    ASSERT(mk_pRendererSettings->iVMR9DitheringLevels <= 0x9F);// the method implementation limit here
                    *reinterpret_cast<__int16*>(m_szDitheringLevel) = 'x0';
                    m_szDitheringLevel[2] = '0' + (static_cast<unsigned __int8>(mk_pRendererSettings->iVMR9DitheringLevels) >> 4);// each hexadecimal char stores 4 bits
                    u8Nibble = mk_pRendererSettings->iVMR9DitheringLevels & 15;// lowest 4 bits
                    u8Nibble += (u8Nibble > 9) ? 'A' - 10 : '0';
                    m_szDitheringLevel[3] = u8Nibble;
                    m_szDitheringLevel[4] = 0;

                    // create single-digit strings
                    ASSERT(mk_pRendererSettings->iVMR9ColorManagementEnable <= 9);// the method implementation limit here
                    *m_szColorManagementLevel = mk_pRendererSettings->iVMR9ColorManagementEnable + '0';
                    ASSERT(mk_pRendererSettings->iVMR9DitheringTestEnable <= 1);// boolean limit
                    *m_szDitheringTest = mk_pRendererSettings->iVMR9DitheringTestEnable + '0';

                    ASSERT(!m_pUtilD3DBlob && !m_pFinalPixelShader);
                    if (FAILED(hr = m_fnD3DCompile(gk_szFinalpassShader, gk_u32LenFinalpassShader, nullptr, m_aShaderMacros, nullptr, "main", m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &m_pUtilD3DBlob, nullptr))
                            || FAILED(hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(m_pUtilD3DBlob->GetBufferPointer()), &m_pFinalPixelShader))) {
                        ErrBox(hr, L"compiling the final pass pixel shader failed\n");
                    }
                    m_pUtilD3DBlob->Release();
                    m_pUtilD3DBlob = nullptr;
                }

                // Compile the subtitle and OSD pass pixel shaders if necessary
                if (!m_pSubtitlePassPixelShader || m_u8VMR9ChromaFixCurrent != mk_pRendererSettings->iVMR9ChromaFix) {// note: m_u8VMR9ChromaFixCurrent can be written by the mixer to indicate that the colorimetry of the input has changed
                    ASSERT(!m_pOSDPassPixelShader);// created as a pair
                    ASSERT(!m_pUtilD3DBlob);
                    if (FAILED(hr = m_fnD3DCompile(gk_szSubtitlePassShader, gk_u32LenSubtitlePassShader, nullptr, m_aShaderMacros, nullptr, "main", m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &m_pUtilD3DBlob, nullptr))
                            || FAILED(hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(m_pUtilD3DBlob->GetBufferPointer()), &m_pSubtitlePassPixelShader))) {
                        ErrBox(hr, L"compiling the subtitle pass pixel shader failed\n");
                    }
                    m_pUtilD3DBlob->Release();
                    m_pUtilD3DBlob = nullptr;
                    if (FAILED(hr = m_fnD3DCompile(gk_szOSDPassShader, gk_u32LenOSDPassShader, nullptr, m_aShaderMacros, nullptr, "main", m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &m_pUtilD3DBlob, nullptr))
                            || FAILED(hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(m_pUtilD3DBlob->GetBufferPointer()), &m_pOSDPassPixelShader))) {
                        ErrBox(hr, L"compiling the OSD pass pixel shader failed\n");
                    }
                    m_pUtilD3DBlob->Release();
                    m_pUtilD3DBlob = nullptr;
                    goto CreateInitialPassShaders;// skip the next checks, when m_pSubtitlePassPixelShader isn't available or the mixer has changed m_u8VMR9ChromaFixCurrent to indicate that the colorimetry of the input has changed, the initial pass items need to be renewed as well
                }

                // Compile the intial pass pixel shaders if necessary
                if (m_bVMR9DisableInitialColorMixingCurrent != mk_pRendererSettings->iVMR9DisableInitialColorMixing || m_u8VMR9ChromaFixCurrent != mk_pRendererSettings->iVMR9ChromaFix || m_u8ChromaTypeTest != m_u8ChromaType) {
CreateInitialPassShaders:
                    m_bVMR9DisableInitialColorMixingCurrent = mk_pRendererSettings->iVMR9DisableInitialColorMixing;
                    m_u8VMR9ChromaFixCurrent = mk_pRendererSettings->iVMR9ChromaFix;
                    m_u8ChromaTypeTest = m_u8ChromaType;
                    if (m_pIniatialPixelShader2) {
                        m_pIniatialPixelShader2->Release();
                        m_pIniatialPixelShader2 = nullptr;
                        if (m_pIniatialPixelShader0) {
                            m_pIniatialPixelShader0->Release();
                            m_pIniatialPixelShader0 = nullptr;
                            if (m_pIniatialPixelShader1) {
                                m_pIniatialPixelShader1->Release();
                                m_pIniatialPixelShader1 = nullptr;
                            }
                        }
                    }

                    ASSERT(!m_pUtilD3DBlob && !m_pIniatialPixelShader2 && !m_pIniatialPixelShader1 && !m_pIniatialPixelShader0);
                    if (!mk_pRendererSettings->iVMR9DisableInitialColorMixing) {
                        unsigned __int8 u8Method = m_u8VMR9ChromaFixCurrent;
                        // handle the horizontal chroma up-sampling for 4:2:2 and 4:2:0
                        if (u8Method && m_u8ChromaType && ((m_pRenderersData->m_dwPCIVendor == PCIV_AMD) || (m_pRenderersData->m_dwPCIVendor == PCIV_ATI) || (m_pRenderersData->m_dwPCIVendor == PCIV_INTEL))) {
                            static_assert(_countof(gk_aszInitialPassShader) == 28, "initial pass pixel shader count changed, adapt the initial pass section accordingly");
                            if ((u8Method <= 2) && ((m_u8ChromaType & 0xF) == 2)) {// special case for the single-pass 4:2:0 types
                                u8Method += 26;// count of gk_aszInitialPassShader, minus the amount of single-pass 4:2:0 types
                            }
                            --u8Method;// no shader for nearest neighbor, so the list is offset by one
                            ASSERT(!m_pUtilD3DBlob);
                            if (FAILED(hr = m_fnD3DCompile(gk_aszInitialPassShader[u8Method], gk_au32LenInitialPassShader[u8Method], nullptr, m_aShaderMacros, nullptr, "main", m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &m_pUtilD3DBlob, nullptr))
                                    || FAILED(hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(m_pUtilD3DBlob->GetBufferPointer()), &m_pIniatialPixelShader2))) {
                                ErrBox(hr, L"compiling initial pass pixel shader 2 failed\n");
                            }
                            m_pUtilD3DBlob->Release();
                            m_pUtilD3DBlob = nullptr;
                            if (m_u8VMR9ChromaFixCurrent > 2) {// exclude the single-pass types
                                if (FAILED(hr = m_fnD3DCompile(gk_szRGBconvYCCShader, gk_u32LenRGBconvYCCShader, nullptr, m_aShaderMacros, nullptr, "main", m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &m_pUtilD3DBlob, nullptr))
                                        || FAILED(hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(m_pUtilD3DBlob->GetBufferPointer()), &m_pIniatialPixelShader0))) {
                                    ErrBox(hr, L"compiling initial pass pixel shader 0 failed\n");
                                }
                                m_pUtilD3DBlob->Release();
                                m_pUtilD3DBlob = nullptr;
                                if ((m_u8ChromaType & 0xF) == 2) {// handle the vertical chroma up-sampling for 4:2:0
                                    u8Method = m_u8VMR9ChromaFixCurrent + 11;// change the +11 when more items are added (11 is based on the first entry for two-pass 4:2:0 shaders, gk_szInitialPassShader14, minus its index for the chroma up-sampling setting, 3)
                                    if (FAILED(hr = m_fnD3DCompile(gk_aszInitialPassShader[u8Method], gk_au32LenInitialPassShader[u8Method], nullptr, m_aShaderMacros, nullptr, "main", m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &m_pUtilD3DBlob, nullptr))
                                            || FAILED(hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(m_pUtilD3DBlob->GetBufferPointer()), &m_pIniatialPixelShader1))) {
                                        ErrBox(hr, L"compiling initial pass pixel shader 1 failed\n");
                                    }
                                    m_pUtilD3DBlob->Release();
                                    m_pUtilD3DBlob = nullptr;
                                }
                            }
                        } else {
                            ASSERT(!m_pUtilD3DBlob);
                            if (FAILED(hr = m_fnD3DCompile(gk_szInitialGammaShader, gk_u32LenInitialGammaShader, nullptr, m_aShaderMacros, nullptr, "main", m_pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &m_pUtilD3DBlob, nullptr))
                                    || FAILED(hr = m_pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(m_pUtilD3DBlob->GetBufferPointer()), &m_pIniatialPixelShader2))) {
                                ErrBox(hr, L"compiling the basic initial pass pixel shader failed\n");
                            }
                            m_pUtilD3DBlob->Release();
                            m_pUtilD3DBlob = nullptr;
                        }
                    }
                }
            }
        }
    }

    size_t upWindowPassCount = m_apCustomPixelShaders[1].GetCount();// amount of custom window pixel shaders
    // prepare temporary window surfaces if required
    if (!m_apTempWindowTexture[1]) {
        size_t upWindowPassCountE = upWindowPassCount;
        if (m_dfSurfaceType != D3DFMT_X8R8G8B8) {
            ++upWindowPassCountE;
            goto PossiblyCreateWindowPassTextures;
        }
        if (upWindowPassCountE) {
PossiblyCreateWindowPassTextures:
            if (upWindowPassCountE > 1) {// activated when more than one pass on window size textures occurs
                if (FAILED(hr = m_pD3DDev->CreateTexture(m_dpPParam.BackBufferWidth, m_dpPParam.BackBufferHeight, 1, D3DUSAGE_RENDERTARGET, m_dfSurfaceType, D3DPOOL_DEFAULT, &m_apTempWindowTexture[1], nullptr))) {
                    ErrBox(hr, L"creation of temporary window texture 1 failed\n");
                }
                ASSERT(!m_apTempWindowSurface[1]);
                if (FAILED(hr = m_apTempWindowTexture[1]->GetSurfaceLevel(0, &m_apTempWindowSurface[1]))) {
                    ErrBox(hr, L"loading surface from temporary window texture 1 failed\n");
                }
            }
            if (!m_apTempWindowTexture[0]) {// activated when at least one pass on window size textures occurs, a pass is made for each set window size phase pixel shader, and by the pass for the the final pass phase
                if (FAILED(hr = m_pD3DDev->CreateTexture(m_dpPParam.BackBufferWidth, m_dpPParam.BackBufferHeight, 1, D3DUSAGE_RENDERTARGET, m_dfSurfaceType, D3DPOOL_DEFAULT, &m_apTempWindowTexture[0], nullptr))) {
                    ErrBox(hr, L"creation of temporary window texture 0 failed\n");
                }
                ASSERT(!m_apTempWindowSurface[0]);
                if (FAILED(hr = m_apTempWindowTexture[0]->GetSurfaceLevel(0, &m_apTempWindowSurface[0]))) {
                    ErrBox(hr, L"loading surface from temporary window texture 0 failed\n");
                }
            }
        }
    }

    size_t upVideoPassCount = m_apCustomPixelShaders[0].GetCount();// amount of custom video pixel shaders
    // prepare temporary video surfaces if required
    if (!m_apTempVideoTexture[1]) {
        size_t upVideoPassCountE = upVideoPassCount;
        if (m_pPreResizerHorizontalPixelShader) {
            ++upVideoPassCountE;
        }
        if (m_pPreResizerVerticalPixelShader) {
            ++upVideoPassCountE;
        }
        if (m_pIniatialPixelShader2) {
            ++upVideoPassCountE;
            if (m_pIniatialPixelShader0) {
                goto CreateVideoPass1Texture;// two inital pass sets, both textures are required
            }
            goto PossiblyCreateVideoPassTextures;
        }
        if (upVideoPassCountE) {
PossiblyCreateVideoPassTextures:
            if (upVideoPassCountE > 1) {// activated when more than one pass on video size textures occurs
CreateVideoPass1Texture:
                if (FAILED(hr = m_pD3DDev->CreateTexture(m_u32VideoWidth, m_u32VideoHeight, 1, D3DUSAGE_RENDERTARGET, m_dfSurfaceType, D3DPOOL_DEFAULT, &m_apTempVideoTexture[1], nullptr))) {
                    ErrBox(hr, L"creation of temporary video texture 1 failed\n");
                }
                ASSERT(!m_apTempVideoSurface[1]);
                if (FAILED(hr = m_apTempVideoTexture[1]->GetSurfaceLevel(0, &m_apTempVideoSurface[1]))) {
                    ErrBox(hr, L"loading surface from temporary video texture 1 failed\n");
                }
            }
            if (!m_apTempVideoTexture[0]) {// activated when at least one pass on video size textures occurs, a pass is made for each set video size phase pixel shader, and by the 0 to 3 passes for the the initial pass phase
                if (FAILED(hr = m_pD3DDev->CreateTexture(m_u32VideoWidth, m_u32VideoHeight, 1, D3DUSAGE_RENDERTARGET, m_dfSurfaceType, D3DPOOL_DEFAULT, &m_apTempVideoTexture[0], nullptr))) {
                    ErrBox(hr, L"creation of temporary video texture 0 failed\n");
                }
                ASSERT(!m_apTempVideoSurface[0]);
                if (FAILED(hr = m_apTempVideoTexture[0]->GetSurfaceLevel(0, &m_apTempVideoSurface[0]))) {
                    ErrBox(hr, L"loading surface from temporary video texture 0 failed\n");
                }
            }
        }
    }

    {
        EXECUTE_ASSERT(QueryPerformanceCounter(&m_liLastPerfCnt));
        m_dPaintTime = static_cast<double>(m_liLastPerfCnt.QuadPart - m_i64PerfCntInit) * m_dPerfFreqr - m_dPrevStartPaint;

        // set up the subtitle texture and read the queue depth, also record the queue depth for the stats screen and use it to allow a delay for the subtitle renderer to catch up
        double dCurrentQueueDepth = m_dCurrentQueueDepth;// also used for m_adQueueDepthHistory, see past the following scope for its recording
        m_dCurrentQueueDepth = 0.0;// the recorded queue depth has to be set to 0, unless the following conditions are met
        __int32 i32Waitablems = 0;
        if (u8RPFlags & FRENDERPAINT_NORMAL) {// parts that should not be activated in paused, frame stepping or menu screen modes
            i32Waitablems = static_cast<__int32>((dSyncOffsetAvrg - 0.5 * m_dDetectedVideoTimePerFrame) * 1000.0);// statistic average synchronization minus half a frame time, added the present queue depth (added later)

            // set pixel shader constants
            if (upVideoPassCount || upWindowPassCount) {
                static float const kfLarge = 8388608.0f;
                static float const kfMedium = 65536.0f;
                static __declspec(align(16)) float const kfOne = 1.0f;
                __m128 xCounter = _mm_load_ss(&m_fPixelShaderCounter);
                // the FP math formats are limited in precision, for both one added bit is implicit and one bit is reserved here as it's often lost with even simple math
                __int64 i64ShaderClock;
                __m128 xInput;
                if (m_dcCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0)) {// FP32 has 23 bits of mantissa
                    i64ShaderClock = (1i64 << (23 + 23)) - 1;
                    xInput = _mm_load_ss(&kfLarge);
                } else {// FP24 has 16 bits of mantissa
                    i64ShaderClock = (1i64 << (16 + 23)) - 1;
                    xInput = _mm_load_ss(&kfMedium);
                }
                xCounter = _mm_add_ss(xCounter, *reinterpret_cast<__m128 const*>(&kfOne));// add one to counter
                xInput = _mm_cmpnlt_ss(xInput, xCounter);// control flow; if the counter is equal or larger than 8388608 or 65536, reset it to 0
                xCounter = _mm_and_ps(xCounter, xInput);
                // store the values to both the video pass and window pass sets
                _mm_store_ss(&m_fPixelShaderCounter, xCounter);
                _mm_store_ss(&m_fPixelShaderCounterCopy, xCounter);

                i64ShaderClock &= m_i64Now;// bitmask out high bits
                m_fShaderClock = m_fShaderClockCopy = static_cast<float>(static_cast<double>(i64ShaderClock) * 0.0000001);// 100 ns to s units, this multiplication causes an exponent shift (calculated log2) of -23.253496664211536435092236006426 (23 bits can be considered to be behind the radix point), using a single-precision binary floating-point directly would truncate the 23+23 bits from the input even before calculation
            }

            // statistics
            if (bPresentEx) {// Vista and newer only, used in windowed mode with the desktop composition enabled or in fullscreen mode
                if (!m_u8OSVersionMinor && m_dpPParam.Windowed) {
                    DWM_TIMING_INFO dtInfo;
                    dtInfo.cbSize = sizeof(DWM_TIMING_INFO);
                    if (SUCCEEDED(m_fnDwmGetCompositionTimingInfo(m_dpPParam.hDeviceWindow, &dtInfo))) {// Vista DWM: use the DWM info, note: this function doesn't activate at first
                        double dQueueRefTime = static_cast<double>(static_cast<__int64>(dtInfo.qpcCompose) - m_i64PerfCntInit) * m_dPerfFreqr;// the standard converter only does a proper job with signed values
                        if (dtInfo.cFramesPending >= 24) {
                            TRACE(L"Video renderer received odd statistics for scheduling, expect heavy frame dropping: %llu pending frames\n", dtInfo.cFramesPending);
                        }
                        double dQueued = static_cast<double>(static_cast<INT>(dtInfo.cFramesPending));// the amount of screen refreshes in between the end of the queue and the frame the statistics are gathered from, the standard converter only does a proper job with signed values
                        double dQueueTime = dQueued * m_dDetectedRefreshTime;
                        // average over thirty-three frames
                        double dQueueDepthSum = dQueueTime + dCurrentQueueDepth;
                        ptrdiff_t i = 31;
                        do {
                            dQueueDepthSum += m_adQueueDepthHistory[i];
                        } while (--i >= 0);
                        dQueueRefTime += (1.0 / 33.0) * dQueueDepthSum;
                        if ((m_dfSurfaceType != D3DFMT_X8R8G8B8) && (m_dcCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0)) && mk_pRendererSettings->iVMR9FrameInterpolation) {// read the actual value of iVMR9FrameInterpolation, the last stored value isn't always updated
                            dQueueRefTime += m_dDetectedVideoTimePerFrame + m_dDetectedVideoTimePerFrame;// the constant frame interpolator has a two frame offset, correct the time difference
                        }

                        dVBlankTime = (dQueueRefTime > m_dPrevVBlankTime) ? dQueueRefTime : m_dPrevVBlankTime;// just a safety net
                        i32Waitablems += static_cast<__int32>(dQueueTime * 1000.0);
                        // record the queue stats
                        m_dCurrentQueueDepth = dQueueTime;
                        if (m_dMaxQueueDepth < dQueueTime) {
                            m_dMaxQueueDepth = dQueueTime;
                        }
                    }
                } else {
                    UINT uiPresentCount;
                    D3DPRESENTSTATS dpStats;
                    if (SUCCEEDED(m_pSwapChain->GetLastPresentCount(&uiPresentCount)) &&
                            SUCCEEDED(m_pSwapChain->GetPresentStats(&dpStats))) {// read stats from the external queue
                        double dQueueRefTime = static_cast<double>(dpStats.SyncQPCTime.QuadPart - m_i64PerfCntInit) * m_dPerfFreqr;
                        double dQueued = static_cast<double>(static_cast<INT>(uiPresentCount - dpStats.PresentCount));// the amount of screen presents in between the end of the queue and the last presented one, the standard converter only does a proper job with signed values
                        double dQueueTime = dQueued * m_dDetectedRefreshTime;
                        if (mk_pRendererSettings->iEVRAlternativeScheduler || ((m_dfSurfaceType != D3DFMT_X8R8G8B8) && (m_dcCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0)) && mk_pRendererSettings->iVMR9FrameInterpolation)) {
                            if ((dpStats.PresentRefreshCount - dpStats.SyncRefreshCount) + (uiPresentCount - dpStats.PresentCount) >= 24) {
                                TRACE(L"Video renderer received odd statistics for scheduling, expect heavy frame dropping: %u refreshes + %u presents lasting one refresh queued\n", dpStats.PresentRefreshCount - dpStats.SyncRefreshCount, uiPresentCount - dpStats.PresentCount);
                            }
                            double dRefreshDiff = static_cast<double>(static_cast<INT>(dpStats.PresentRefreshCount - dpStats.SyncRefreshCount));// the amount of screen refreshes in between the last presented frame and the frame the statistics are gathered from, the standard converter only does a proper job with signed values
                            dQueueTime += dRefreshDiff * m_dDetectedRefreshTime;// assuming a present lasts only one refresh here
                        } else {
                            if ((uiPresentCount - dpStats.PresentCount) >= 24) {
                                TRACE(L"Video renderer received odd statistics for scheduling, expect heavy frame dropping: %u presents queued\n", uiPresentCount - dpStats.PresentCount);
                            }
                        }
                        // average over thirty-three frames
                        double dQueueDepthSum = dQueueTime + dCurrentQueueDepth;
                        ptrdiff_t i = 31;
                        do {
                            dQueueDepthSum += m_adQueueDepthHistory[i];
                        } while (--i >= 0);
                        dQueueRefTime += (1.0 / 33.0) * dQueueDepthSum;
                        if ((m_dfSurfaceType != D3DFMT_X8R8G8B8) && (m_dcCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0)) && mk_pRendererSettings->iVMR9FrameInterpolation) {// read the actual value of iVMR9FrameInterpolation, the last stored value isn't always updated
                            dQueueRefTime += m_dDetectedVideoTimePerFrame + m_dDetectedVideoTimePerFrame;// the constant frame interpolator has a two frame offset, correct the time difference
                        }

                        dVBlankTime = (dQueueRefTime > m_dPrevVBlankTime) ? dQueueRefTime : m_dPrevVBlankTime;// just a safety net
                        i32Waitablems += static_cast<__int32>(dQueueTime * 1000.0);
                        // record the queue stats
                        m_dCurrentQueueDepth = dQueueTime;
                        if (m_dMaxQueueDepth < dQueueTime) {
                            m_dMaxQueueDepth = dQueueTime;
                        }
                    }
                }
            } else if ((m_dfSurfaceType != D3DFMT_X8R8G8B8) && (m_dcCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0)) && mk_pRendererSettings->iVMR9FrameInterpolation) {// read the actual value of iVMR9FrameInterpolation, the last stored value isn't always updated
                dVBlankTime += m_dDetectedVideoTimePerFrame + m_dDetectedVideoTimePerFrame;// the constant frame interpolator has a two frame offset, correct the time difference
            }

            if (mk_bIsEVR) {
                static_cast<CEVRAllocatorPresenter*>(this)->OnVBlankFinished(dVBlankTime);
            }
            double dJitter = dVBlankTime - m_dPrevVBlankTime;
            // filter out seeking
            if (abs(dJitter) < (8.0 * m_dDetectedVideoTimePerFrame)) {
                size_t upFrames = NB_JITTER - 1;
                double dFrames = 1.0 / NB_JITTER;
                if (m_upDetectedFrameTimePos < NB_JITTER - 1) {
                    upFrames = m_upDetectedFrameTimePos;
                    dFrames = 1.0 / (static_cast<double>(static_cast<ptrdiff_t>(upFrames)) + 1.0);// the standard converter only does a proper job with signed values
                }

                m_adJitter[upCurrJitterPos] = dJitter;
                m_dMaxJitter = -2147483648.0;
                m_dMinJitter = 2147483648.0;

                // calculate the real FPS
                double dJitterSum = 0.0;
                ptrdiff_t i = upFrames;
                do {
                    dJitterSum += m_adJitter[i];
                } while (--i >= 0);
                m_dJitterMean = dJitterSum * dFrames;
                double dJitterMean = m_dJitterMean;
                double dDeviationSum = 0.0;
                i = upFrames;
                do {
                    double dDevInt = m_adJitter[i] - dJitterMean;
                    double dDeviation = static_cast<double>(m_adJitter[i]) - m_dJitterMean;
                    dDeviationSum += dDeviation * dDeviation;
                    if (m_dMaxJitter < dDevInt) {
                        m_dMaxJitter = dDevInt;
                    }
                    if (m_dMinJitter > dDevInt) {
                        m_dMinJitter = dDevInt;
                    }
                } while (--i >= 0);
                m_dJitterStdDev = sqrt(dDeviationSum * dFrames);

                m_dAverageFrameRate = 1.0 / (dFrames * dJitterSum);
                if (!mk_bIsEVR) {// not an ideal detection, as this belongs in the mixer schedulers, but it does a pretty good job though
                    m_dModeratedTimeSpeed = m_dAverageFrameRate * dJitter;
                }

                m_adPaintTimeO[upCurrJitterPos] = m_dPaintTime;
                // calculate the paint time average
                double dPaintTimeSum = 0.0;
                i = upFrames;
                do {
                    dPaintTimeSum += m_adPaintTimeO[i];
                } while (--i >= 0);
                m_dPaintTimeMean = dPaintTimeSum * dFrames;

                if (m_dPaintTimeMin > m_dPaintTime) {
                    m_dPaintTimeMin = m_dPaintTime;
                }
                if (m_dPaintTimeMax < m_dPaintTime) {
                    m_dPaintTimeMax = m_dPaintTime;
                }
            } else {
                m_dModeratedTimeSpeed = 1.0;
                m_dAverageFrameRate = 0.0;
                m_dPaintTime = m_dPaintTimeMax;
            }

            // finalize a normal frame
            m_dPrevVBlankTime = dVBlankTime;
            ++m_upDetectedFrameTimePos;// this only increments, so it will wrap around, on a 32-bit integer it's once a year with 120 fps video
        }
        // store previous scheduling depth
        unsigned __int8 u8Spos = m_u8SchedulerPos & 31;// modulo by low bitmask
        m_adQueueDepthHistory[u8Spos] = dCurrentQueueDepth;// this one is actually from the previous frame, m_dCurrentQueueDepth contains the current value
        m_u8SchedulerPos = 1 + u8Spos;// m_u8SchedulerPos is even incremented in paused mode by this design

        // retrieve a subtitle
        ASSERT(!m_pSubtitleTexture);
        if (CBSubPic* pSubPic = m_pSubPicQueue->LookupSubPic(m_i64Now, i32Waitablems)) {
            if (pSubPic->GetSourceAndDest(&m_rcSubSrc)) {// also writes m_rcSubDst
                IDirect3DTexture9* pSubtitleTexture = static_cast<CDX9SubPic*>(pSubPic)->m_pTexture;
                m_pSubtitleTexture = pSubtitleTexture;
                pSubtitleTexture->AddRef();
            }
            pSubPic->Release();
        }
    }

    m_pD3DDev->BeginScene();// begin a new frame
    // set up the texture received from the mixer
    ASSERT(m_apVideoTexture[m_u8CurrentMixerSurface]);
    hr = m_pD3DDev->SetTexture(0, m_apVideoTexture[m_u8CurrentMixerSurface]);
    unsigned __int8 u8Vsrc = 1, u8Vdst = 0;// for swappable video textures and surfaces

    // apply color conversion and optionally the chroma fix
    if ((m_dfSurfaceType != D3DFMT_X8R8G8B8) && m_pIniatialPixelShader2) {// if m_pIniatialPixelShader2 isn't available, the function is completely disabled
        if (m_pIniatialPixelShader0) {
            hr = m_pD3DDev->SetPixelShader(m_pIniatialPixelShader0);
            ASSERT(m_apTempVideoSurface[0]);
            hr = m_pD3DDev->SetRenderTarget(0, m_apTempVideoSurface[0]);// the very first is allowed to skip the swap this way
            hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 4, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 4
            hr = m_pD3DDev->SetTexture(0, m_apTempVideoTexture[0]);
            if (m_pIniatialPixelShader1) {
                hr = m_pD3DDev->SetPixelShader(m_pIniatialPixelShader1);
                ASSERT(m_apTempVideoSurface[1]);
                hr = m_pD3DDev->SetRenderTarget(0, m_apTempVideoSurface[1]);
                hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 4, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 4
                hr = m_pD3DDev->SetTexture(0, m_apTempVideoTexture[1]);
            } else {
                u8Vsrc = 0;
                u8Vdst = 1;
            }
        }
        hr = m_pD3DDev->SetPixelShader(m_pIniatialPixelShader2);
        ASSERT(m_apTempVideoSurface[u8Vdst]);
        hr = m_pD3DDev->SetRenderTarget(0, m_apTempVideoSurface[u8Vdst]);
        hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 4, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 4
        hr = m_pD3DDev->SetTexture(0, m_apTempVideoTexture[u8Vdst]);
        unsigned __int8 u8Temp = u8Vsrc;
        u8Vsrc = u8Vdst;
        u8Vdst = u8Temp;
    }

    // apply the custom video size pixel shaders
    if (upVideoPassCount) {
        hr = m_pD3DDev->SetPixelShaderConstantF(0, &m_fVideoWidth, 2);// specialized set of floats

        POSITION pos = m_apCustomPixelShaders[0].GetHeadPosition();
        do {
            EXTERNALSHADER& ceShader = m_apCustomPixelShaders[0].GetNext(pos);
            ASSERT(ceShader.pPixelShader);
            hr = m_pD3DDev->SetPixelShader(ceShader.pPixelShader);
            ASSERT(m_apTempVideoSurface[u8Vdst]);
            hr = m_pD3DDev->SetRenderTarget(0, m_apTempVideoSurface[u8Vdst]);
            hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 4, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 4
            hr = m_pD3DDev->SetTexture(0, m_apTempVideoTexture[u8Vdst]);
            unsigned __int8 u8Temp = u8Vsrc;
            u8Vsrc = u8Vdst;
            u8Vdst = u8Temp;
        } while (pos);
    }

    // resize the frame
    // in some cases, this pass isn't required at all
    bool bResizerSection = false;
    if (m_VideoRect.left || m_VideoRect.top// repositioning required
            || !m_bNoHresize || !m_bNoVresize// resizing required
            || (m_u32VideoWidth != m_dpPParam.BackBufferWidth) || (m_u32VideoHeight != m_dpPParam.BackBufferHeight)// width or height difference
            || ((m_dfSurfaceType == D3DFMT_X8R8G8B8) && !upWindowPassCount)) {// the final pass and the video size pixel shaders can both render to the back buffer, if these are not available, you must render to the back buffer in this pass
        bResizerSection = true;

        // pre-process resizing if required
        // note: these two can only exist when m_pResizerPixelShaderX is available as well
        if (m_pPreResizerHorizontalPixelShader) {
            hr = m_pD3DDev->SetPixelShader(m_pPreResizerHorizontalPixelShader);
            ASSERT(m_apTempVideoSurface[u8Vdst]);
            hr = m_pD3DDev->SetRenderTarget(0, m_apTempVideoSurface[u8Vdst]);
            hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 4, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 4
            hr = m_pD3DDev->SetTexture(0, m_apTempVideoTexture[u8Vdst]);
            unsigned __int8 u8Temp = u8Vsrc;
            u8Vsrc = u8Vdst;
            u8Vdst = u8Temp;
        }
        if (m_pPreResizerVerticalPixelShader) {
            hr = m_pD3DDev->SetPixelShader(m_pPreResizerVerticalPixelShader);
            ASSERT(m_apTempVideoSurface[u8Vdst]);
            hr = m_pD3DDev->SetRenderTarget(0, m_apTempVideoSurface[u8Vdst]);
            hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 4, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 4
            hr = m_pD3DDev->SetTexture(0, m_apTempVideoTexture[u8Vdst]);
            unsigned __int8 u8Temp = u8Vsrc;
            u8Vsrc = u8Vdst;
            u8Vdst = u8Temp;
        }

        IDirect3DSurface9* pResizerTarget = ((m_dfSurfaceType == D3DFMT_X8R8G8B8) && !upWindowPassCount) ? m_pBackBuffer : m_apTempWindowSurface[0];
        ASSERT(pResizerTarget);
        if (m_pResizerPixelShaderY) {// 2-pass resizing
            ASSERT(m_pResizerPixelShaderX);
            hr = m_pD3DDev->SetPixelShader(m_pResizerPixelShaderX);
            ASSERT(m_pIntermediateResizeSurface);
            hr = m_pD3DDev->SetRenderTarget(0, m_pIntermediateResizeSurface);
            hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 12, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 12
            ASSERT(m_pIntermediateResizeTexture);
            hr = m_pD3DDev->SetTexture(0, m_pIntermediateResizeTexture);
            ASSERT(m_pResizerPixelShaderY);
            hr = m_pD3DDev->SetPixelShader(m_pResizerPixelShaderY);
            hr = m_pD3DDev->SetRenderTarget(0, pResizerTarget);
            hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 16, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 16
        } else {// 1-pass resizing
            if (m_pResizerPixelShaderX) {// resize by pixel shader
                hr = m_pD3DDev->SetPixelShader(m_pResizerPixelShaderX);
                hr = m_pD3DDev->SetRenderTarget(0, pResizerTarget);
                hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 8, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 8
            } else {// resize by StretchRect()
                // note: this check doesn't have to include m_pPreResizerHorizontalPixelShader and m_pPreResizerVerticalPixelShader, as those require m_pResizerPixelShaderX to be active
                IDirect3DSurface9* pResizerSource = (upVideoPassCount || m_pIniatialPixelShader2) ? m_apTempVideoSurface[u8Vsrc] : m_apVideoSurface[m_u8CurrentMixerSurface];
                ASSERT(pResizerSource);
                hr = m_pD3DDev->StretchRect(pResizerSource, &m_rcResizerStretchRectSrc, pResizerTarget, &m_rcResizerStretchRectDst, m_u8ActiveResizer ? D3DTEXF_LINEAR : D3DTEXF_POINT);
                hr = m_pD3DDev->SetRenderTarget(0, pResizerTarget);// required for OSD, subtitle and stats screen blending on top of this surface
            }
            // clear artifacts, 2-pass resizing has m_pIntermediateResizeTexture that obsucures left and right
            if (m_rcClearLeft.bottom) {
                m_pD3DDev->ColorFill(pResizerTarget, &m_rcClearLeft, mk_pRendererSettings->dwBackgoundColor);
            }
            if (m_rcClearRight.bottom) {
                m_pD3DDev->ColorFill(pResizerTarget, &m_rcClearRight, mk_pRendererSettings->dwBackgoundColor);
            }
        }
        // clear artifacts, common part
        if (m_rcClearTop.right) {
            m_pD3DDev->ColorFill(pResizerTarget, &m_rcClearTop, mk_pRendererSettings->dwBackgoundColor);
        }
        if (m_rcClearBottom.right) {
            m_pD3DDev->ColorFill(pResizerTarget, &m_rcClearBottom, mk_pRendererSettings->dwBackgoundColor);
        }
    } else if (!m_pIniatialPixelShader2 && !upVideoPassCount) {// the initial pass and the video size pixel shaders already correctly set a render target
        ASSERT(m_apVideoSurface[m_u8CurrentMixerSurface]);
        hr = m_pD3DDev->SetRenderTarget(0, m_apVideoSurface[m_u8CurrentMixerSurface]);// required for OSD, subtitle and stats screen blending on top of this surface
    }

    unsigned __int8 u8Wsrc = 0, u8Wdst = 1;// for swappable window textures and surfaces
    // apply the custom window size pixel shaders
    if (upWindowPassCount) {
        hr = m_pD3DDev->SetPixelShaderConstantF(0, &m_fWindowWidth, 3);// specialized set of floats
        if (bResizerSection) {// can inherit from the resizer section or the video size parts directly
            ASSERT(m_apTempWindowTexture[0]);
            hr = m_pD3DDev->SetTexture(0, m_apTempWindowTexture[0]);
        }

        POSITION pos = m_apCustomPixelShaders[1].GetHeadPosition();
        for (;;) {
            EXTERNALSHADER& ceShader = m_apCustomPixelShaders[1].GetNext(pos);
            ASSERT(ceShader.pPixelShader);
            hr = m_pD3DDev->SetPixelShader(ceShader.pPixelShader);
            IDirect3DSurface9* pRenderTarget = (pos || (m_dfSurfaceType != D3DFMT_X8R8G8B8)) ? m_apTempWindowSurface[u8Wdst] : m_pBackBuffer;
            ASSERT(pRenderTarget);
            hr = m_pD3DDev->SetRenderTarget(0, pRenderTarget);
            hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 0

            unsigned __int8 u8Temp = u8Wsrc;
            u8Wsrc = u8Wdst;
            u8Wdst = u8Temp;
            if (!pos) {
                break;
            }
            ASSERT(m_apTempWindowTexture[u8Wsrc]);
            hr = m_pD3DDev->SetTexture(0, m_apTempWindowTexture[u8Wsrc]);
        }
    }

    if (m_pSubtitleTexture || m_pRenderersData->m_fDisplayStats || m_pOSDTexture) {
        // I've never seen SetRenderState() or SetSamplerState() return anything but S_OK for valid states, so a simple EXECUTE_ASSERT on S_OK test is fine here.
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE)));

        // alpha blend the subtitles
        if (m_pSubtitleTexture) {
            ASSERT((m_dfSurfaceType == D3DFMT_X8R8G8B8) || m_pSubtitlePassPixelShader);// m_pSubtitlePassPixelShader has to be created with the final pass
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetPixelShader(m_pSubtitlePassPixelShader)));// may be nullptr when there's no final pass active

            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetTexture(0, m_pSubtitleTexture)));
            if (!m_rcSubDst.left && !m_rcSubDst.top && (m_rcSubDst.right == static_cast<LONG>(m_dpPParam.BackBufferWidth)) && (m_rcSubDst.bottom == static_cast<LONG>(m_dpPParam.BackBufferHeight))) {
                EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2)));// draw the rectangle, BaseVertexIndex: 0, for fullscreen pictures from the subtitle renderer
            } else {// TODO: add resizing shaders
                if ((m_rcSubDstTest.left != m_rcSubDst.left) || (m_rcSubDstTest.top != m_rcSubDst.top) || (m_rcSubDstTest.right != m_rcSubDst.right) || (m_rcSubDstTest.bottom != m_rcSubDst.bottom) || !m_pSubBlendVBuffer) {
                    m_rcSubDstTest = m_rcSubDst;
                    if (m_pSubBlendVBuffer) {
                        m_pSubBlendVBuffer->Release();
                        m_pSubBlendVBuffer = nullptr;
                    }

                    // create the special vertex buffer
                    ASSERT(!m_pSubBlendVBuffer);
                    if (FAILED(hr = m_pD3DDev->CreateVertexBuffer(sizeof(CUSTOMVERTEX_TEX1[4]), D3DUSAGE_DONOTCLIP | D3DUSAGE_WRITEONLY, D3DFVF_XYZRHW | D3DFVF_TEX1, D3DPOOL_DEFAULT, &m_pSubBlendVBuffer, nullptr))) {
                        ErrBox(hr, L"creating the subtitle vertex buffer failed\n");
                    }
                    void* pVoid;
                    if (FAILED(hr = m_pSubBlendVBuffer->Lock(0, 0, &pVoid, 0))) {
                        ErrBox(hr, L"locking the subtitle vertex buffer failed\n");
                    }

                    float rdl = static_cast<float>(m_rcSubDst.left + m_i32SubWindowOffsetLeft), rdr = static_cast<float>(m_rcSubDst.right + m_i32SubWindowOffsetLeft), rdt = static_cast<float>(m_rcSubDst.top + m_i32SubWindowOffsetTop), rdb = static_cast<float>(m_rcSubDst.bottom + m_i32SubWindowOffsetTop);
                    if (mk_pRendererSettings->nSPCMaxRes == 1) {// adapt three-quarter-sized subtitle texture
                        rdl *= 1.0f / 0.75f;
                        rdt *= 1.0f / 0.75f;
                        rdr *= 1.0f / 0.75f;
                        rdb *= 1.0f / 0.75f;
                    } else if (mk_pRendererSettings->nSPCMaxRes > 1) {// adapt half-sized subtitle texture
                        rdl *= 2.0f;
                        rdt *= 2.0f;
                        rdr *= 2.0f;
                        rdb *= 2.0f;
                    }
                    rdl += -0.5f;
                    rdt += -0.5f;
                    rdr += -0.5f;
                    rdb += -0.5f;

                    // note: size specification of this array is used for allocating the buffer
                    __declspec(align(16)) CUSTOMVERTEX_TEX1 v[4] = {
                        {rdl, rdt, 0.5f, 2.0f, 0.0f, 0.0f},
                        {rdr, rdt, 0.5f, 2.0f, 1.0f, 0.0f},
                        {rdl, rdb, 0.5f, 2.0f, 0.0f, 1.0f},
                        {rdr, rdb, 0.5f, 2.0f, 1.0f, 1.0f}
                    };

                    float* pDst = reinterpret_cast<float*>(pVoid), *pSrc = &v[0].x;
                    ASSERT(!(reinterpret_cast<uintptr_t>(pDst) & 15));// if not 16-byte aligned, _mm_stream_ps will fail
                    __m128 x0 = _mm_load_ps(pSrc);
                    __m128 x1 = _mm_load_ps(pSrc + 4);
                    __m128 x2 = _mm_load_ps(pSrc + 8);
                    __m128 x3 = _mm_load_ps(pSrc + 12);
                    __m128 x4 = _mm_load_ps(pSrc + 16);
                    __m128 x5 = _mm_load_ps(pSrc + 20);
                    _mm_stream_ps(pDst, x0);
                    _mm_stream_ps(pDst + 4, x1);
                    _mm_stream_ps(pDst + 8, x2);
                    _mm_stream_ps(pDst + 12, x3);
                    _mm_stream_ps(pDst + 16, x4);
                    _mm_stream_ps(pDst + 20, x5);
                    if (FAILED(hr = m_pSubBlendVBuffer->Unlock())) {
                        ErrBox(hr, L"unlocking the subtitle vertex buffer failed\n");
                    }
                }

                // set the special vertex buffer
                bool bResized = (m_rcSubSrc.right - m_rcSubSrc.left != m_rcSubDst.right - m_rcSubDst.left) || (m_rcSubSrc.bottom - m_rcSubSrc.top != m_rcSubDst.bottom - m_rcSubDst.top);
                if (bResized) {
                    EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR)));
                    EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR)));
                }
                EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetStreamSource(0, m_pSubBlendVBuffer, 0, sizeof(CUSTOMVERTEX_TEX1))));
                EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2)));// draw the rectangle, BaseVertexIndex: 0

                // cleanup, set the normal vertex buffer
                hr = m_pD3DDev->SetStreamSource(0, m_pVBuffer, 0, sizeof(CUSTOMVERTEX_TEX1));
                if (bResized) {
                    EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT)));
                    EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT)));
                }
            }

            m_pSubtitleTexture->Release();// invalidate immediately after using it
            m_pSubtitleTexture = nullptr;
        }

        // alpha blend the stats screen
        if (m_pRenderersData->m_fDisplayStats) {
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetPixelShader(nullptr)));
            // select full or limited range color palette
            STATSSCREENCOLORS const* pksscColors = ((m_dfSurfaceType == D3DFMT_A2R10G10B10) || (m_dfSurfaceType == D3DFMT_A16B16G16R16)) ? gk_sscColorsets + 1 : gk_sscColorsets;

            // initialize stats screen utility functions
            if (!m_pFont) {
                UINT uiFontBase = m_dpPParam.BackBufferWidth >> 6;
                UINT uiFontHeight = uiFontBase + (uiFontBase >> 2) + (uiFontBase >> 3);
                UINT uiFontWidth, uiFontWeight;
                if (m_dpPParam.BackBufferWidth < 1120) {
                    uiFontWidth = uiFontBase >> 1;// 1/128
                    uiFontWeight = FW_NORMAL;
                } else {
                    uiFontWidth = (uiFontBase >> 2) + (uiFontBase >> 3) + (uiFontBase >> 4);// a little bit thinner
                    uiFontWeight = FW_BOLD;
                }
                if (FAILED(hr = m_fnD3DXCreateFontW(m_pD3DDev, uiFontHeight, uiFontWidth, uiFontWeight, 1, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Lucida Console", &m_pFont))) {
                    ErrBox(hr, L"creating stats screen font failed\n");
                }
                if (FAILED(hr = m_pFont->PreloadCharacters(0, 255))) {
                    ErrBox(hr, L"preloading stats screen font characters failed\n");
                }

                // create the special vertex buffer
                ASSERT(!m_pStatsRectVBuffer);
                if (FAILED(hr = m_pD3DDev->CreateVertexBuffer(sizeof(CUSTOMVERTEX_COLOR[8]), D3DUSAGE_DONOTCLIP | D3DUSAGE_WRITEONLY, D3DFVF_DIFFUSE | D3DFVF_XYZRHW | D3DFVF_TEX0, D3DPOOL_DEFAULT, &m_pStatsRectVBuffer, nullptr))) {
                    ErrBox(hr, L"creating the stats screen vertex buffer failed\n");
                }
                void* pVoid;
                if (FAILED(hr = m_pStatsRectVBuffer->Lock(0, 0, &pVoid, 0))) {
                    ErrBox(hr, L"locking the stats screen vertex buffer failed\n");
                }

                // jitter graph rectangle, note: the same parameters are used for the SSE part to initialze the lines later on
                float fStartX = m_fWindowWidth - 1024.0f, fStartY = m_fWindowHeight - 300.0f, fEndX = m_fWindowWidth, fEndY = m_fWindowHeight - 40.0f;

                float vtfsr = m_fWindowWidth - 0.5f, vtfsb = m_fWindowHeight - 0.5f, // vertex data needs offsets of -0.5
                      vtjgl = fStartX - 0.5f, vtjgt = fStartY - 0.5f, vtjgr = fEndX - 0.5f, vtjgb = fEndY - 0.5f;
                __int32 Color = pksscColors->background;

                // note: size specification of this array is used for allocating the buffer
                __declspec(align(16)) CUSTOMVERTEX_COLOR v[8] = {
                    { -0.5f, -0.5f, 0.5f, 2.0f, Color}, // 0: full screen
                    {vtfsr, -0.5f, 0.5f, 2.0f, Color},
                    { -0.5f, vtfsb, 0.5f, 2.0f, Color},
                    {vtfsr, vtfsb, 0.5f, 2.0f, Color},
                    {vtjgl, vtjgt, 0.5f, 2.0f, Color},// 4: only the jitter graph
                    {vtjgr, vtjgt, 0.5f, 2.0f, Color},
                    {vtjgl, vtjgb, 0.5f, 2.0f, Color},
                    {vtjgr, vtjgb, 0.5f, 2.0f, Color}
                };

                float* pDst = reinterpret_cast<float*>(pVoid), *pSrc = &v[0].x;
                ASSERT(!(reinterpret_cast<uintptr_t>(pDst) & 15));// if not 16-byte aligned, _mm_stream_ps will fail
                __m128 x0 = _mm_load_ps(pSrc);
                __m128 x1 = _mm_load_ps(pSrc + 4);
                __m128 x2 = _mm_load_ps(pSrc + 8);
                __m128 x3 = _mm_load_ps(pSrc + 12);
                __m128 x4 = _mm_load_ps(pSrc + 16);
                __m128 x5 = _mm_load_ps(pSrc + 20);
                __m128 x6 = _mm_load_ps(pSrc + 24);
                __m128 x7 = _mm_load_ps(pSrc + 28);
                __m128 x8 = _mm_load_ps(pSrc + 32);
                __m128 x9 = _mm_load_ps(pSrc + 36);
                _mm_stream_ps(pDst, x0);
                _mm_stream_ps(pDst + 4, x1);
                _mm_stream_ps(pDst + 8, x2);
                _mm_stream_ps(pDst + 12, x3);
                _mm_stream_ps(pDst + 16, x4);
                _mm_stream_ps(pDst + 20, x5);
                _mm_stream_ps(pDst + 24, x6);
                _mm_stream_ps(pDst + 28, x7);
                _mm_stream_ps(pDst + 32, x8);
                _mm_stream_ps(pDst + 36, x9);
                if (FAILED(hr = m_pStatsRectVBuffer->Unlock())) {
                    ErrBox(hr, L"unlocking the stats screen vertex buffer failed\n");
                }

                // initialize lines
                static __declspec(align(16)) float const afOffsetsV[4] = {0.0f, -300.0f, 0.0f, 0.0f};
                pDst = m_afStatsBarsJGraph;
                __m128 xBaseM = _mm_load_ps(&m_fWindowWidth);// actually cheaper than loading only 8 bytes
                __m128 xBSO = _mm_set_ss(-48.0f);
                xBaseM = _mm_add_ps(xBaseM, *reinterpret_cast<__m128 const*>(afOffsetsV));
                __m128 xBLO = _mm_set_ss(-1024.0f);
                xBaseM = _mm_movelh_ps(xBaseM, xBaseM);
                __m128 xBaseS = _mm_add_ps(xBaseM, xBSO);
                xBaseM = _mm_add_ps(xBaseM, xBLO);
                __m128 xBaseL = _mm_add_ps(xBaseS, xBLO);

                static __declspec(align(16)) float const pLTen[4] = {0.0f, 10.0f, 0.0f, 10.0f};
                __m128 xTen = _mm_load_ps(pLTen);
                __m128 xCounter = _mm_setzero_ps();
                unsigned __int8 i = 2;
                unsigned __int8 j = 3;
                do {// 10 50 90, 170 210
                    xCounter = _mm_add_ps(xCounter, xTen);
                    _mm_store_ps(pDst, _mm_add_ps(xBaseM, xCounter));
                    pDst += 4;
LinesSubLoop:
                    unsigned __int8 k = 3;
                    do {// 20 30 40, 60 70 80, 100 110 120, 140 150 160, 180 190 200, 220 230 240
                        xCounter = _mm_add_ps(xCounter, xTen);
                        _mm_store_ps(pDst, _mm_add_ps(xBaseS, xCounter));
                        pDst += 4;
                    } while (--k);
                } while (--j);
                xCounter = _mm_add_ps(xCounter, xTen);
                if (!(--i)) {
                    goto LinesBreakLoop;
                }
                _mm_store_ps(pDst, _mm_add_ps(xBaseL, xCounter));// 130
                pDst += 4;
                j = 3;
                goto LinesSubLoop;
LinesBreakLoop:
                _mm_store_ps(pDst, _mm_add_ps(xBaseM, xCounter));// 250
            }
            if (!m_pLine) {// cleared less often than m_pFont
                if (FAILED(hr = m_fnD3DXCreateLine(m_pD3DDev, &m_pLine))) {
                    ErrBox(hr, L"creating stats screen line failed\n");
                }
                EXECUTE_ASSERT(S_OK == (hr = m_pLine->SetWidth(2.5f)));
                EXECUTE_ASSERT(S_OK == (hr = m_pLine->SetAntialias(TRUE)));
            }

            // set the special vertex state and buffer
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetFVF(D3DFVF_DIFFUSE | D3DFVF_XYZRHW | D3DFVF_TEX0)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetStreamSource(0, m_pStatsRectVBuffer, 0, sizeof(CUSTOMVERTEX_COLOR))));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetTexture(0, nullptr)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, (m_pRenderersData->m_fDisplayStats == 1) ? 0 : 4, 0, 4, 0, 2)));// draw the rectangle, BaseVertexIndex: 0 or 4, depending on the type of active stats screen

            if (m_pRenderersData->m_fDisplayStats < 3) {
                // note: this expression is used everywhere to assert that the buffer is more than big enough at all times: ASSERT(pwcBuff - awcStats < 4096 - 128);
                // note: while creating this string, none of the intermediate passes are expected to end the string with a 0, none of the passes require such a string type, nor does the function that takes the final string
                wchar_t awcStats[5120];
                wchar_t* pwcBuff = awcStats;
                int iBufferTaken;

                if (m_pRenderersData->m_fDisplayStats == 1) {
                    iBufferTaken = _swprintf(pwcBuff, L"Settings     : OS %C.%C.%hu, Rszr %hu, ", m_u8OSVersionMajor + '0', m_u8OSVersionMinor + '0', m_u16OSVersionBuild, static_cast<unsigned __int16>(m_u8DX9ResizerTest));// there's currenly no 8 bits %?u mask unfortunately
                    ASSERT(iBufferTaken > 0);
                    pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                    ASSERT(pwcBuff - awcStats < 4096 - 128);

                    if (mk_bIsEVR) {
                        static wchar_t const kszEVRLabel[] = L"EVR";
                        memcpy(pwcBuff, kszEVRLabel, sizeof(kszEVRLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszEVRLabel) - 1;
                        ASSERT(pwcBuff - awcStats < 4096 - 128);

                        if (mk_pRendererSettings->iEVRAlternativeScheduler && (m_boCompositionEnabled || (!m_dpPParam.Windowed && (m_u8OSVersionMajor >= 6)))) {// Vista and newer only
                            static wchar_t const kszASchdlLabel[] = L", A. Schdl";
                            memcpy(pwcBuff, kszASchdlLabel, sizeof(kszASchdlLabel) - 2);// chop off end 0
                            pwcBuff += _countof(kszASchdlLabel) - 1;
                            ASSERT(pwcBuff - awcStats < 4096 - 128);
                        }
                    } else {
                        static wchar_t const kszVMR9Label[] = L"VMR-9";
                        memcpy(pwcBuff, kszVMR9Label, sizeof(kszVMR9Label) - 2);// chop off end 0
                        pwcBuff += _countof(kszVMR9Label) - 1;
                        ASSERT(pwcBuff - awcStats < 4096 - 128);

                        if (mk_pRendererSettings->fVMR9MixerYUV && (m_u8OSVersionMajor < 6)) {// Not available on Vista and newer
                            static wchar_t const kszYUVMLabel[] = L", YUV M.";
                            memcpy(pwcBuff, kszYUVMLabel, sizeof(kszYUVMLabel) - 2);// chop off end 0
                            pwcBuff += _countof(kszYUVMLabel) - 1;
                            ASSERT(pwcBuff - awcStats < 4096 - 128);
                        }
                    }

                    if (mk_pRendererSettings->iVMRDisableDesktopComposition) {
                        static wchar_t const kszNoDCLabel[] = L", No DC";
                        memcpy(pwcBuff, kszNoDCLabel, sizeof(kszNoDCLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszNoDCLabel) - 1;
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    }

                    if (!m_dpPParam.Windowed) {
                        static wchar_t const kszD3DFSLabel[] = L", D3D FS";
                        memcpy(pwcBuff, kszD3DFSLabel, sizeof(kszD3DFSLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszD3DFSLabel) - 1;
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    }

                    if (m_bAlternativeVSync) {
                        iBufferTaken = _swprintf(pwcBuff, L", A. VSync Offset %d", mk_pRendererSettings->iVMR9VSyncOffset);
                        ASSERT(iBufferTaken > 0);
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                        ASSERT(pwcBuff - awcStats < 4096 - 128);

                        if (mk_pRendererSettings->iVMRFlushGPUBeforeVSync) {
                            static wchar_t const kszFBVSLabel[] = L", F. B. VSync";
                            memcpy(pwcBuff, kszFBVSLabel, sizeof(kszFBVSLabel) - 2);// chop off end 0
                            pwcBuff += _countof(kszFBVSLabel) - 1;
                            ASSERT(pwcBuff - awcStats < 4096 - 128);

                            if (mk_pRendererSettings->iVMRFlushGPUWait) {
                                static wchar_t const kszFVSWaitLabel[] = L", F. Wait";
                                memcpy(pwcBuff, kszFVSWaitLabel, sizeof(kszFVSWaitLabel) - 2);// chop off end 0
                                pwcBuff += _countof(kszFVSWaitLabel) - 1;
                                ASSERT(pwcBuff - awcStats < 4096 - 128);
                            }
                        }
                    }

                    if (m_dfSurfaceType != D3DFMT_X8R8G8B8) {
                        if (m_u8VMR9ColorManagementEnableCurrent == 1) {
                            static wchar_t const kszColorMLabel[] = L", Color M.";
                            memcpy(pwcBuff, kszColorMLabel, sizeof(kszColorMLabel) - 2);// chop off end 0
                            pwcBuff += _countof(kszColorMLabel) - 1;
                            ASSERT(pwcBuff - awcStats < 4096 - 128);
                        } else if (m_u8VMR9ColorManagementEnableCurrent == 2) {
                            static wchar_t const kszLimSDRLabel[] = L", Lim. SD C. Ranges";
                            memcpy(pwcBuff, kszLimSDRLabel, sizeof(kszLimSDRLabel) - 2);// chop off end 0
                            pwcBuff += _countof(kszLimSDRLabel) - 1;
                            ASSERT(pwcBuff - awcStats < 4096 - 128);
                        } else if (m_u8VMR9ColorManagementEnableCurrent == 3) {
                            static wchar_t const kszLimHDRLabel[] = L", Lim. HD C. Ranges";
                            memcpy(pwcBuff, kszLimHDRLabel, sizeof(kszLimHDRLabel) - 2);// chop off end 0
                            pwcBuff += _countof(kszLimHDRLabel) - 1;
                            ASSERT(pwcBuff - awcStats < 4096 - 128);
                        }

                        if (m_u8VMR9DitheringLevelsCurrent) {
                            iBufferTaken = _swprintf(pwcBuff, L", Dither L. %hu", m_u8VMR9DitheringLevelsCurrent);
                            ASSERT(iBufferTaken > 0);
                            pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                            ASSERT(pwcBuff - awcStats < 4096 - 128);
                        }

                        if ((m_dcCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0)) && m_u8VMR9FrameInterpolationCurrent) {
                            iBufferTaken = _swprintf(pwcBuff, L", Frame I. %hu", m_u8VMR9FrameInterpolationCurrent);
                            ASSERT(iBufferTaken > 0);
                            pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                            ASSERT(pwcBuff - awcStats < 4096 - 128);
                        }
                    }
                    *pwcBuff = L'\n';
                    pwcBuff += 1;
                    ASSERT(pwcBuff - awcStats < 4096 - 128);

                    if (m_upLenstrD3D9Device) {
                        static wchar_t const kszRenderDevLabel[] = L"Render device: ";
                        memcpy(pwcBuff, kszRenderDevLabel, sizeof(kszRenderDevLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszRenderDevLabel) - 1;
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                        // variable-length short string copy without alignment guarantees over a single wchar_t; just issue rep movsw
                        __movsw(reinterpret_cast<unsigned __int16*>(pwcBuff), reinterpret_cast<unsigned __int16*>(m_awcD3D9Device), m_upLenstrD3D9Device);
                        pwcBuff += m_upLenstrD3D9Device;
                        *pwcBuff = L'\n';
                        pwcBuff += 1;
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    }

                    if (int len = m_strDecoder.GetLength()) {
                        static wchar_t const kszDecoderLabel[] = L"Decoder      : ";
                        memcpy(pwcBuff, kszDecoderLabel, sizeof(kszDecoderLabel) - 2);// chop off end 0
                        pwcBuff += _countof(kszDecoderLabel) - 1;
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                        // variable-length short string copy without alignment guarantees over a single wchar_t; just issue rep movsw
                        __movsw(reinterpret_cast<unsigned __int16*>(pwcBuff), reinterpret_cast<unsigned __int16 const*>(static_cast<wchar_t const*>(m_strDecoder)), static_cast<size_t>(static_cast<unsigned int>(len)));
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(len));
                        *pwcBuff = L'\n';
                        pwcBuff += 1;
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    }

                    // part one: construct L"DXVA?        : ";
                    memcpy(pwcBuff, GetDXVAVersion(), 5 * 2);// note: GetDXVAVersion() always returns a string of 5 characters
                    pwcBuff += 5;
                    static wchar_t const kszDXVAPaddingLabel[] = L"        : ";
                    memcpy(pwcBuff, kszDXVAPaddingLabel, sizeof(kszDXVAPaddingLabel) - 2);// chop off end 0
                    pwcBuff += _countof(kszDXVAPaddingLabel) - 1;
                    ASSERT(pwcBuff - awcStats < 4096 - 128);
                    // part two: copy description
                    size_t upStringLength;
                    LPCTSTR szDXVAdd = GetDXVADecoderDescription(&upStringLength);
                    // variable-length short string copy without alignment guarantees over a single wchar_t; just issue rep movsw
                    __movsw(reinterpret_cast<unsigned __int16*>(pwcBuff), reinterpret_cast<unsigned __int16 const*>(szDXVAdd), upStringLength);
                    pwcBuff += upStringLength;
                    *pwcBuff = L'\n';
                    pwcBuff += 1;
                    ASSERT(pwcBuff - awcStats < 4096 - 128);

                    if (int len = m_strMixerStatus.GetLength()) {
                        // variable-length short string copy without alignment guarantees over a single wchar_t; just issue rep movsw
                        __movsw(reinterpret_cast<unsigned __int16*>(pwcBuff), reinterpret_cast<unsigned __int16 const*>(static_cast<wchar_t const*>(m_strMixerStatus)), static_cast<size_t>(static_cast<unsigned int>(len)));
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(len));
                        // note: m_strMixerStatus contains a partial string
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                        if (m_dfSurfaceType == D3DFMT_X8R8G8B8) {
                            static wchar_t const kszU8STLabel[] = L"X8R8G8B8\n";
                            memcpy(pwcBuff, kszU8STLabel, sizeof(kszU8STLabel) - 2);// chop off end 0
                            pwcBuff += _countof(kszU8STLabel) - 1;
                            ASSERT(pwcBuff - awcStats < 4096 - 128);
                        } else if (m_dfSurfaceType == D3DFMT_A2R10G10B10) {
                            static wchar_t const kszU10STLabel[] = L"A2R10G10B10\n";
                            memcpy(pwcBuff, kszU10STLabel, sizeof(kszU10STLabel) - 2);// chop off end 0
                            pwcBuff += _countof(kszU10STLabel) - 1;
                            ASSERT(pwcBuff - awcStats < 4096 - 128);
                        } else if (m_dfSurfaceType == D3DFMT_A16B16G16R16F) {
                            static wchar_t const kszF16STLabel[] = L"A16B16G16R16F\n";
                            memcpy(pwcBuff, kszF16STLabel, sizeof(kszF16STLabel) - 2);// chop off end 0
                            pwcBuff += _countof(kszF16STLabel) - 1;
                            ASSERT(pwcBuff - awcStats < 4096 - 128);
                        } else {// the mixer surface type for the 16-bit normalized unsigned integer type is 32-bit floating point, due to the mixer handling of the color intervals
                            ASSERT(m_dfSurfaceType == D3DFMT_A16B16G16R16);
                            static wchar_t const kszF32STLabel[] = L"A32B32G32R32F\n";
                            memcpy(pwcBuff, kszF32STLabel, sizeof(kszF32STLabel) - 2);// chop off end 0
                            pwcBuff += _countof(kszF32STLabel) - 1;
                            ASSERT(pwcBuff - awcStats < 4096 - 128);
                        }
                    }

                    iBufferTaken = _swprintf(pwcBuff, L"Buffering    : Mixer surfaces amount %hu, Current surface %2hu, Free surfaces %2hu\nFormats      : Surface %s, Display %s\nVideo size   : %u×%u, Aspect ratio %u:%u\nWindow size  : %u×%u, Video area %u×%u\n",
                                             static_cast<unsigned __int16>(m_u8MixerSurfaceCount), static_cast<unsigned __int16>(m_u8CurrentMixerSurface), static_cast<unsigned __int16>(m_u8MixerSurfaceCount - m_u8MixerSurfacesUsed),
                                             GetSurfaceFormatName(m_dfSurfaceType), GetSurfaceFormatName(m_dpPParam.BackBufferFormat),
                                             m_u32VideoWidth, m_u32VideoHeight, m_u32AspectRatioWidth, m_u32AspectRatioHeight,
                                             m_dpPParam.BackBufferWidth, m_dpPParam.BackBufferHeight, m_VideoRect.right - m_VideoRect.left, m_VideoRect.bottom - m_VideoRect.top);
                    ASSERT(iBufferTaken > 0);
                    pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                    ASSERT(pwcBuff - awcStats < 4096 - 128);

                    if (*m_szMonitorName) {// if the first character is 0, this data is unavailable (aquired from the EDID in the registry by ReadDisplay())
                        iBufferTaken = _swprintf(pwcBuff, L"Monitor EDID : %s, Native resolution %hu×%hu, Display size %hu×%hu mm\n", m_szMonitorName, m_u16MonitorHorRes, m_u16MonitorVerRes, m_u16mmMonitorWidth, m_u16mmMonitorHeight);
                        ASSERT(iBufferTaken > 0);
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    }

                    iBufferTaken = _swprintf(pwcBuff, L"Frame rate   : Avr %.06f Hz, Ref %.06f Hz %c, FrameT %6.3f ms, StdDev %6.3f ms, Clock %6.2f%%", m_dAverageFrameRate, m_dStreamReferenceVideoFrameRate, m_bInterlaced ? L'I' : L'P', m_dDetectedVideoTimePerFrame * 1000.0, m_dDetectedFrameTimeStdDev * 1000.0, m_dModeratedTimeSpeed * 100.0);
                    ASSERT(iBufferTaken > 0);
                    pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                    ASSERT(pwcBuff - awcStats < 4096 - 128);
                    if (m_bDetectedLock) {
                        iBufferTaken = _swprintf(pwcBuff, L", FrameR Lock %.06f Hz", m_dDetectedVideoFrameRate);
                        ASSERT(iBufferTaken > 0);
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    }
                    // the \n in front is intentional
                    iBufferTaken = _swprintf(pwcBuff, L"\nRefresh rate : %.06f Hz, Ref %.06f Hz, Scan lines %d", m_dDetectedRefreshRate, m_dReferenceRefreshRate, static_cast<__int32>(m_dDetectedScanlinesPerFrame + 0.5));
                    ASSERT(iBufferTaken > 0);
                    pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                    ASSERT(pwcBuff - awcStats < 4096 - 128);
                    if (!mk_bIsEVR) {
                        *pwcBuff = L'\n';
                        pwcBuff += 1;
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    } else {// only CEVRAllocatorPresenter has the code to support m_dLastFrameDuration, Frame Time Correction and m_bSyncStatsAvailable
                        iBufferTaken = _swprintf(pwcBuff, L" Last duration %6.3f ms, Corrected frame time ", m_dLastFrameDuration * 1000.0);
                        ASSERT(iBufferTaken > 0);
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                        ASSERT(pwcBuff - awcStats < 4096 - 128);

                        if (m_bCorrectedFrameTime) {
                            static wchar_t const kszCFTYesLabel[] = L"Yes\n";
                            memcpy(pwcBuff, kszCFTYesLabel, sizeof(kszCFTYesLabel) - 2);// chop off end 0
                            pwcBuff += _countof(kszCFTYesLabel) - 1;
                            ASSERT(pwcBuff - awcStats < 4096 - 128);
                        } else {
                            static wchar_t const kszCFTNoLabel[] = L"No\n";
                            memcpy(pwcBuff, kszCFTNoLabel, sizeof(kszCFTNoLabel) - 2);// chop off end 0
                            pwcBuff += _countof(kszCFTNoLabel) - 1;
                            ASSERT(pwcBuff - awcStats < 4096 - 128);
                        }

                        if (m_bSyncStatsAvailable) {
                            iBufferTaken = _swprintf(pwcBuff, L"Sync offset  : Min %+7.3f ms, Max %+7.3f ms, StdDev %6.3f ms, Avr %+7.3f ms, Mode %C\n", m_dMinSyncOffset * 1000.0, m_dMaxSyncOffset * 1000.0, m_dSyncOffsetStdDev * 1000.0, m_dSyncOffsetAvr * 1000.0, m_u8VSyncMode + '0');
                            ASSERT(iBufferTaken > 0);
                            pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                            ASSERT(pwcBuff - awcStats < 4096 - 128);
                        }
                    }

                    iBufferTaken = _swprintf(pwcBuff, L"Jitter       : Min %+7.3f ms, Max %+7.3f ms, StdDev %6.3f ms\n", m_dMinJitter * 1000.0, static_cast<double>(m_dMaxJitter) * 1000.0, m_dJitterStdDev * 1000.0);
                    ASSERT(iBufferTaken > 0);
                    pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                    ASSERT(pwcBuff - awcStats < 4096 - 128);

                    if (m_bAlternativeVSync) {
                        iBufferTaken = _swprintf(pwcBuff, L"VBlank wait  : Start %5d, End %5d, Wait %6.3f ms, Lock %6.3f ms, Offset %5d, Max %5d, End present %5d\n", m_i32VBlankStartWait, m_i32VBlankEndWait, m_dVBlankWaitTime * 1000.0, m_dVBlankLockTime * 1000.0, m_i32VBlankMin, m_i32VBlankMax - m_i32VBlankMin, m_i32VBlankEndPresent);
                        ASSERT(iBufferTaken > 0);
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    } else {
                        iBufferTaken = _swprintf(pwcBuff, L"Queue depth  : Current %6.3f ms, Max %6.3f ms\n", m_dCurrentQueueDepth * 1000.0, m_dMaxQueueDepth * 1000.0);
                        ASSERT(iBufferTaken > 0);
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    }

                    if (m_dWaitForGPUTime) {
                        iBufferTaken = _swprintf(pwcBuff, L"Paint time   : Draw %6.3f ms, Min %6.3f ms, Max %6.3f ms, GPU %6.3f ms\n", (m_dPaintTime - m_dWaitForGPUTime) * 1000.0, m_dPaintTimeMin * 1000.0, m_dPaintTimeMax * 1000.0, m_dWaitForGPUTime * 1000.0);
                        ASSERT(iBufferTaken > 0);
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    } else {
                        iBufferTaken = _swprintf(pwcBuff, L"Paint time   : Draw %6.3f ms, Min %6.3f ms, Max %6.3f ms\n", m_dPaintTime * 1000.0, m_dPaintTimeMin * 1000.0, m_dPaintTimeMax * 1000.0);
                        ASSERT(iBufferTaken > 0);
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    }

                    if (m_dRasterStatusWaitTimeMax) {
                        iBufferTaken = _swprintf(pwcBuff, L"Raster status: Wait %6.3f ms, Min %6.3f ms, Max %6.3f ms\n", m_dRasterStatusWaitTime * 1000.0, m_dRasterStatusWaitTimeMin * 1000.0, m_dRasterStatusWaitTimeMax * 1000.0);
                        ASSERT(iBufferTaken > 0);
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    }

                    SubPicQueueStats QStats;
                    m_pSubPicQueue->GetStats(&QStats);
                    unsigned __int8 u8SubPics = QStats.u8SubPics;
                    iBufferTaken = _swprintf(pwcBuff, L"Subtitles    : Buffered %2hu, Queue start %+8.3f s, Queue end %+8.3f s\n", u8SubPics, static_cast<double>(QStats.i64Start) * 0.0000001, static_cast<double>(QStats.i64Stop) * 0.0000001);
                    ASSERT(iBufferTaken > 0);
                    pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                    ASSERT(pwcBuff - awcStats < 4096 - 128);

                    unsigned __int8 i = 0;
                    while (i != u8SubPics) {
                        m_pSubPicQueue->GetStats(i, &QStats.i64Start);
                        iBufferTaken = _swprintf(pwcBuff, L"Subtitle %2hu  : [%I64d, %I64d) ms      ", i, QStats.i64Start / 10000, QStats.i64Stop / 10000);
                        ASSERT(iBufferTaken > 0);
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                        ASSERT(pwcBuff - awcStats < 4096 - 128);

                        if (i & 1) {// make two columns: only append a newline character after each odd
                            *pwcBuff = L'\n';
                            pwcBuff += 1;
                            ASSERT(pwcBuff - awcStats < 4096 - 128);
                        }
                        ++i;
                    }
                } else {
                    iBufferTaken = _swprintf(pwcBuff, L"Frame rate   : Avr %.06f Hz, ", m_dAverageFrameRate);
                    ASSERT(iBufferTaken > 0);
                    pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                    ASSERT(pwcBuff - awcStats < 4096 - 128);
                    // indicate locked or reference frame rate
                    iBufferTaken = _swprintf(pwcBuff, m_bDetectedLock ? L"FrameR Lock %.06f Hz\n" : L"ref %.06f Hz\n", m_dDetectedVideoFrameRate);
                    ASSERT(iBufferTaken > 0);
                    pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                    ASSERT(pwcBuff - awcStats < 4096 - 128);

                    if (m_bAlternativeVSync) {
                        iBufferTaken = _swprintf(pwcBuff, L"VBlank wait  : Start %5d, End %5d, End present %5d\n", m_i32VBlankStartWait, m_i32VBlankEndWait, m_i32VBlankEndPresent);
                        ASSERT(iBufferTaken > 0);
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    } else {
                        iBufferTaken = _swprintf(pwcBuff, L"Queue depth  : Current %6.3f ms, Max %6.3f ms\n", m_dCurrentQueueDepth * 1000.0, m_dMaxQueueDepth * 1000.0);
                        ASSERT(iBufferTaken > 0);
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    }

                    if (m_dWaitForGPUTime) {
                        iBufferTaken = _swprintf(pwcBuff, L"Paint time   : Draw %6.3f ms, GPU %6.3f ms\n", (m_dPaintTime - m_dWaitForGPUTime) * 1000.0, m_dWaitForGPUTime * 1000.0);
                        ASSERT(iBufferTaken > 0);
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    } else {
                        iBufferTaken = _swprintf(pwcBuff, L"Paint time   : Draw %6.3f ms\n", m_dPaintTime * 1000.0);
                        ASSERT(iBufferTaken > 0);
                        pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                        ASSERT(pwcBuff - awcStats < 4096 - 128);
                    }

                    SubPicQueueStats QStats;
                    m_pSubPicQueue->GetStats(&QStats);
                    iBufferTaken = _swprintf(pwcBuff, L"Subtitles    : Buffered %2hu, Queue start %+8.3f s, Queue end %+8.3f s\n", QStats.u8SubPics, static_cast<double>(QStats.i64Start) * 0.0000001, static_cast<double>(QStats.i64Stop) * 0.0000001);
                    ASSERT(iBufferTaken > 0);
                    pwcBuff += static_cast<size_t>(static_cast<unsigned int>(iBufferTaken));
                    ASSERT(pwcBuff - awcStats < 4096 - 128);
                }

                size_t upCharacterCount = pwcBuff - awcStats;
                RECT rc = {5, 3, 0, 0};
                EXECUTE_ASSERT(m_pFont->DrawTextW(nullptr, awcStats, static_cast<int>(upCharacterCount), &rc, DT_NOCLIP | DT_LEFT | DT_TOP, pksscColors->white));
            }

            EXECUTE_ASSERT(S_OK == (hr = m_pLine->Begin()));

            ptrdiff_t i = 24;
            do {
                EXECUTE_ASSERT(S_OK == (hr = m_pLine->Draw(reinterpret_cast<D3DXVECTOR2*>(&m_afStatsBarsJGraph[i << 2]), 2, pksscColors->gray)));
            } while (--i >= 0);
            float fStartY = m_fWindowHeight - 300.0f + 130.0f;// all graphs center on 130
            float Points[NB_JITTER * 2];// this avoids D3DXVECTOR2's useless default initializers
            size_t upBasePos = m_upDetectedFrameTimePos;

            float fPaintTimeMean = static_cast<float>(m_dPaintTimeMean);
            float* pFill = Points;
            float fLineX = m_fWindowWidth;
            float fPTOffset = fPaintTimeMean;
            i = NB_JITTER - 1;
            do {
                pFill[0] = fLineX;
                fLineX -= 4.0f;
                size_t upJ = (upBasePos + i) & (NB_JITTER - 1);// modulo action by low bitmask
                pFill[1] = fStartY + (static_cast<float>(m_adPaintTimeO[upJ]) - fPTOffset) * 2000.0f;
                pFill += 2;
            } while (--i >= 0);
            EXECUTE_ASSERT(S_OK == (hr = m_pLine->Draw(reinterpret_cast<D3DXVECTOR2*>(Points), NB_JITTER, pksscColors->cyan)));

            pFill = Points;
            fLineX = m_fWindowWidth;
            float fJOffset = fPaintTimeMean;
            i = NB_JITTER - 1;
            do {
                pFill[0] = fLineX;
                fLineX -= 4.0f;
                size_t upJ = (upBasePos + i) & (NB_JITTER - 1);// modulo action by low bitmask
                pFill[1] = fStartY + (static_cast<float>(m_adJitter[upJ]) - fJOffset) * 2000.0f;
                pFill += 2;
            } while (--i >= 0);
            EXECUTE_ASSERT(S_OK == (hr = m_pLine->Draw(reinterpret_cast<D3DXVECTOR2*>(Points), NB_JITTER, pksscColors->red)));

            pFill = Points;
            fLineX = m_fWindowWidth;
            i = NB_JITTER - 1;
            do {
                pFill[0] = fLineX;
                fLineX -= 4.0f;
                size_t upJ = (upBasePos + i) & (NB_JITTER - 1);// modulo action by low bitmask
                pFill[1] = fStartY + static_cast<float>(m_adDetectedFrameTimeHistory[upJ] - m_dDetectedVideoTimePerFrame) * 20000.0f;// magnified by 10
                pFill += 2;
            } while (--i >= 0);
            EXECUTE_ASSERT(S_OK == (hr = m_pLine->Draw(reinterpret_cast<D3DXVECTOR2*>(Points), NB_JITTER, pksscColors->magenta)));

            if (((m_dfSurfaceType != D3DFMT_X8R8G8B8) && (m_dcCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0)) && m_u8VMR9FrameInterpolationCurrent) || (bPresentEx && mk_pRendererSettings->iEVRAlternativeScheduler)) {
                float fStartYTop = m_fWindowHeight - 300.0f;
                pFill = Points;
                fLineX = m_fWindowWidth;
                i = NB_JITTER - 1;
                do {
                    pFill[0] = fLineX;
                    fLineX -= 4.0f;
                    size_t upJ = (upBasePos + i) & (NB_JITTER - 1);// modulo action by low bitmask
                    pFill[1] = fStartYTop + static_cast<float>(m_au8PresentCountLog[upJ]) * -25.0f;// 25 pixels per screen refresh, offset from the top
                    pFill += 2;
                } while (--i >= 0);
                EXECUTE_ASSERT(S_OK == (hr = m_pLine->Draw(reinterpret_cast<D3DXVECTOR2*>(Points), NB_JITTER, pksscColors->yellow)));

                pFill = Points;
                fLineX = m_fWindowWidth;
                i = NB_JITTER - 1;
                do {
                    pFill[0] = fLineX;
                    fLineX -= 4.0f;
                    size_t upJ = (upBasePos + i) & (NB_JITTER - 1);// modulo action by low bitmask
                    pFill[1] = fStartYTop + m_afFTdividerILog[upJ] * -25.0f;
                    pFill += 2;
                } while (--i >= 0);
                EXECUTE_ASSERT(S_OK == (hr = m_pLine->Draw(reinterpret_cast<D3DXVECTOR2*>(Points), NB_JITTER, pksscColors->blue)));

                // draw a bottom line to 'support' the graph
                Points[0] = m_fWindowWidth;
                Points[1] = fStartYTop;
                Points[2] = m_fWindowWidth - 1024.0f;
                Points[3] = fStartYTop;
                EXECUTE_ASSERT(S_OK == (hr = m_pLine->Draw(reinterpret_cast<D3DXVECTOR2*>(Points), 2, pksscColors->blue)));
            }

            if (m_bSyncStatsAvailable) {
                upBasePos = m_upNextSyncOffsetPos + 1;// the position of the sync stats has an offset of one
                pFill = Points;
                fLineX = m_fWindowWidth;
                i = NB_JITTER - 1;
                do {
                    pFill[0] = fLineX;
                    fLineX -= 4.0f;
                    size_t upJ = (upBasePos + i) & (NB_JITTER - 1);// modulo action by low bitmask
                    pFill[1] = fStartY + static_cast<float>(m_adSyncOffset[upJ]) * 2000.0f;
                    pFill += 2;
                } while (--i >= 0);
                EXECUTE_ASSERT(S_OK == (hr = m_pLine->Draw(reinterpret_cast<D3DXVECTOR2*>(Points), NB_JITTER, pksscColors->green)));
            }

            EXECUTE_ASSERT(S_OK == (hr = m_pLine->End()));

            // set the normal vertex state and buffer
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetStreamSource(0, m_pVBuffer, 0, sizeof(CUSTOMVERTEX_TEX1))));
        }

        // alpha blend the OSD
        if (m_pOSDTexture) {
            ASSERT((m_dfSurfaceType == D3DFMT_X8R8G8B8) || m_pOSDPassPixelShader);// m_pOSDPassPixelShader has to be created with the final pass
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetPixelShader(m_pOSDPassPixelShader)));// may be nullptr when there's no final pass active
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetTexture(0, m_pOSDTexture)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2)));// draw the rectangle, BaseVertexIndex: 0
        }

        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE)));
        EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE)));
    }

    // constant frame interpolator
    if ((m_dfSurfaceType != D3DFMT_X8R8G8B8) && (m_dcCaps.PixelShaderVersion >= D3DPS_VERSION(3, 0)) && mk_pRendererSettings->iVMR9FrameInterpolation) {
        if (m_apFIPixelShader[3]) {
            if (m_u8VMR9FrameInterpolationCurrent != mk_pRendererSettings->iVMR9FrameInterpolation) {
                m_apFIPixelShader[0]->Release();
                m_apFIPixelShader[0] = nullptr;
                m_apFIPixelShader[1]->Release();
                m_apFIPixelShader[1] = nullptr;
                m_apFIPixelShader[2]->Release();
                m_apFIPixelShader[2] = nullptr;
                m_apFIPixelShader[3]->Release();
                m_apFIPixelShader[3] = nullptr;
                if (m_apFIPrePixelShader[0]) {
                    m_apFIPrePixelShader[0]->Release();
                    m_apFIPrePixelShader[0] = nullptr;
                    m_apFIPrePixelShader[1]->Release();
                    m_apFIPrePixelShader[1] = nullptr;
                    m_apFIPrePixelShader[2]->Release();
                    m_apFIPrePixelShader[2] = nullptr;
                }
                goto RenewFIShaders;
            }
        } else {
RenewFIShaders:
            m_u8VMR9FrameInterpolationCurrent = mk_pRendererSettings->iVMR9FrameInterpolation;
            if (!m_apTempWindowTexture[2]) {
                ptrdiff_t i = 2;
                do {
                    ASSERT(!m_apTempWindowTexture[2 + i] && !m_apTempWindowSurface[2 + i]);
                    if (FAILED(hr = m_pD3DDev->CreateTexture(m_dpPParam.BackBufferWidth, m_dpPParam.BackBufferHeight, 1, D3DUSAGE_RENDERTARGET, m_dfSurfaceType, D3DPOOL_DEFAULT, &m_apTempWindowTexture[2 + i], nullptr))) {
                        ErrBox(hr, L"creation of temporary window texture failed\n");
                    }
                    if (FAILED(hr = m_apTempWindowTexture[2 + i]->GetSurfaceLevel(0, &m_apTempWindowSurface[2 + i]))) {
                        ErrBox(hr, L"loading surface from temporary window texture failed\n");
                    }
                    m_pD3DDev->ColorFill(m_apTempWindowSurface[2 + i], nullptr, 0);
                } while (--i >= 0);
                m_u8FIold = 2;
                m_u8FIprevious = 3;
                m_u8FInext = 4;
            }

            // m_szFrameInterpolationLevel; create single-digit string
            ASSERT(mk_pRendererSettings->iVMR9FrameInterpolation << 1 <= 0x9);// the method implementation limit here
            *m_szFrameInterpolationLevel = (mk_pRendererSettings->iVMR9FrameInterpolation << 1) + '0';// it can be 0, 2, 4, 6 or 8

            HANDLE hProcesses[6];
            SHADERTRANSFORM Transforms[7];
            D3D_SHADER_MACRO* macros = m_aShaderMacros;
            LPCSTR pProfile = m_pProfile;
            D3DCompilePtr fnD3DCompile = m_fnD3DCompile;
            IDirect3DDevice9* pD3DDev = m_pD3DDev;
            SHADERTRANSFORM* pTransformsTmp = Transforms;
            unsigned __int8 j = 7;
            do {
                pTransformsTmp->macros = macros;
                pTransformsTmp->pProfile = pProfile;
                pTransformsTmp->fnD3DCompile = fnD3DCompile;
                pTransformsTmp->pD3DDev = pD3DDev;
                ++pTransformsTmp;
            } while (--j);

            char const* const* pszShaderCode = gk_aszBasicFrameInterpolationShader;
            UINT const* puiShaderCodeLen = gk_au32LenBasicFrameInterpolationShader;
            DWORD dwThreads = 3;
            if (m_u8VMR9FrameInterpolationCurrent != 1) {
                ASSERT(!m_apFIPrePixelShader[0] && !m_apFIPrePixelShader[1] && !m_apFIPrePixelShader[2]);

                Transforms[4].szShaderCode = gk_aszPreAdaptiveFrameInterpolationShader[0];
                Transforms[4].u32ShaderLength = gk_au32LenPreAdaptiveFrameInterpolationShader[0];
                Transforms[4].ppShader = m_apFIPrePixelShader;
                Transforms[5].szShaderCode = gk_aszPreAdaptiveFrameInterpolationShader[1];
                Transforms[5].u32ShaderLength = gk_au32LenPreAdaptiveFrameInterpolationShader[1];
                Transforms[5].ppShader = m_apFIPrePixelShader + 1;
                Transforms[6].szShaderCode = gk_aszPreAdaptiveFrameInterpolationShader[2];
                Transforms[6].u32ShaderLength = gk_au32LenPreAdaptiveFrameInterpolationShader[2];
                Transforms[6].ppShader = m_apFIPrePixelShader + 2;

                hProcesses[3] = ::CreateThread(nullptr, 0x20000, ShadCThreadStatic, Transforms + 4, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                if (!hProcesses[3]) {
                    ErrBox(0, L"failed to create a worker thread for the compiling section of frame blend pre pixel shader 0");
                }
                hProcesses[4] = ::CreateThread(nullptr, 0x20000, ShadCThreadStatic, Transforms + 5, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                if (!hProcesses[4]) {
                    TerminateThread(hProcesses[3], 0xDEAD);
                    CloseHandle(hProcesses[3]);
                    ErrBox(0, L"failed to create a worker thread for the compiling section of frame blend pre pixel shader 1");
                }
                hProcesses[5] = ::CreateThread(nullptr, 0x20000, ShadCThreadStatic, Transforms + 6, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
                if (!hProcesses[5]) {
                    TerminateThread(hProcesses[3], 0xDEAD);
                    CloseHandle(hProcesses[3]);
                    TerminateThread(hProcesses[4], 0xDEAD);
                    CloseHandle(hProcesses[4]);
                    ErrBox(0, L"failed to create a worker thread for the compiling section of frame blend pre pixel shader 2");
                }

                if (!m_apFIPreTexture[3]) {
                    ptrdiff_t i = 3;
                    do {
                        if (FAILED(hr = m_pD3DDev->CreateTexture(m_dpPParam.BackBufferWidth, m_dpPParam.BackBufferHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_G16R16F, D3DPOOL_DEFAULT, &m_apFIPreTexture[i], nullptr))) {
                            ErrBox(hr, L"creation of blend vector texture failed\n");
                        }
                        ASSERT(!m_apFIPreSurface[i]);
                        if (FAILED(hr = m_apFIPreTexture[i]->GetSurfaceLevel(0, &m_apFIPreSurface[i]))) {
                            ErrBox(hr, L"loading surface from frame blend vector texture failed\n");
                        }
                        m_pD3DDev->ColorFill(m_apFIPreSurface[i], nullptr, 0);
                    } while (--i >= 0);
                }
                pszShaderCode = gk_aszAdaptiveFrameInterpolationShader;
                puiShaderCodeLen = gk_au32LenAdaptiveFrameInterpolationShader;
                dwThreads = 6;
            }

            ASSERT(!m_apFIPixelShader[0] && !m_apFIPixelShader[1] && !m_apFIPixelShader[2] && !m_apFIPixelShader[3]);
            Transforms[0].szShaderCode = pszShaderCode[0];
            Transforms[0].u32ShaderLength = puiShaderCodeLen[0];
            Transforms[0].ppShader = m_apFIPixelShader;
            Transforms[1].szShaderCode = pszShaderCode[1];
            Transforms[1].u32ShaderLength = puiShaderCodeLen[1];
            Transforms[1].ppShader = m_apFIPixelShader + 1;
            Transforms[2].szShaderCode = pszShaderCode[2];
            Transforms[2].u32ShaderLength = puiShaderCodeLen[2];
            Transforms[2].ppShader = m_apFIPixelShader + 2;
            Transforms[3].szShaderCode = pszShaderCode[3];
            Transforms[3].u32ShaderLength = puiShaderCodeLen[3];
            Transforms[3].ppShader = m_apFIPixelShader + 3;

            hProcesses[0] = ::CreateThread(nullptr, 0x20000, ShadCThreadStatic, Transforms + 1, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
            if (!hProcesses[0]) {
                if (m_u8VMR9FrameInterpolationCurrent != 1) {
                    TerminateThread(hProcesses[3], 0xDEAD);
                    CloseHandle(hProcesses[3]);
                    TerminateThread(hProcesses[4], 0xDEAD);
                    CloseHandle(hProcesses[4]);
                    TerminateThread(hProcesses[5], 0xDEAD);
                    CloseHandle(hProcesses[5]);
                }
                ErrBox(0, L"failed to create a worker thread for the compiling section of frame blend pixel shader 1");
            }
            hProcesses[1] = ::CreateThread(nullptr, 0x20000, ShadCThreadStatic, Transforms + 2, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
            if (!hProcesses[1]) {
                if (m_u8VMR9FrameInterpolationCurrent != 1) {
                    TerminateThread(hProcesses[3], 0xDEAD);
                    CloseHandle(hProcesses[3]);
                    TerminateThread(hProcesses[4], 0xDEAD);
                    CloseHandle(hProcesses[4]);
                    TerminateThread(hProcesses[5], 0xDEAD);
                    CloseHandle(hProcesses[5]);
                }
                TerminateThread(hProcesses[0], 0xDEAD);
                CloseHandle(hProcesses[0]);
                ErrBox(0, L"failed to create a worker thread for the compiling section of frame blend pixel shader 2");
            }
            hProcesses[2] = ::CreateThread(nullptr, 0x20000, ShadCThreadStatic, Transforms + 3, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
            if (!hProcesses[2]) {
                if (m_u8VMR9FrameInterpolationCurrent != 1) {
                    TerminateThread(hProcesses[3], 0xDEAD);
                    CloseHandle(hProcesses[3]);
                    TerminateThread(hProcesses[4], 0xDEAD);
                    CloseHandle(hProcesses[4]);
                    TerminateThread(hProcesses[5], 0xDEAD);
                    CloseHandle(hProcesses[5]);
                }
                TerminateThread(hProcesses[0], 0xDEAD);
                CloseHandle(hProcesses[0]);
                TerminateThread(hProcesses[1], 0xDEAD);
                CloseHandle(hProcesses[1]);
                ErrBox(0, L"failed to create a worker thread for the compiling section of frame blend pixel shader 3");
            }

            // process one shader compile in this thread (an easy one)
            ShadCThreadStatic(Transforms);

            if (WaitForMultipleObjects(dwThreads, hProcesses, TRUE, 300000) == WAIT_TIMEOUT) {// 5 minutes should be much more than enough
                ASSERT(0);
                ptrdiff_t i = dwThreads - 1;
                do {
                    TerminateThread(hProcesses[i], 0xDEAD);
                    CloseHandle(hProcesses[i]);
                } while (--i >= 0);
                ErrBox(0, L"the compiling section of frame blend pixel shaders failed to complete a worker thread task within 5 minutes");
            }
            ptrdiff_t i = dwThreads - 1;
            do {
                EXECUTE_ASSERT(CloseHandle(hProcesses[i]));
            } while (--i >= 0);

            if (!m_apFIPixelShader[0] || !m_apFIPixelShader[1] || !m_apFIPixelShader[2] || !m_apFIPixelShader[3]) {
                ErrBox(0, L"compiling frame blend pixel shader failed\n");
            }
        }

        ASSERT(m_apTempWindowSurface[u8Wsrc]);// required for both Pass1InterFrames and Pass2InterFrames steps
        if (!bResizerSection && !upWindowPassCount) {// no resizer or custom window pixel shaders, make this part inherit from the video section instead
            // these textures and surfaces have the same amount of reference counts, be careful with this pass though, it's very sensitive to errors
            if (upVideoPassCount || m_pIniatialPixelShader2) {// regular swap with only temp surfaces
                ASSERT(m_apTempVideoSurface[u8Vsrc]);
                IDirect3DSurface9* pTmpS = m_apTempVideoSurface[u8Vsrc];
                m_apTempVideoSurface[u8Vsrc] = m_apTempWindowSurface[u8Wsrc];
                m_apTempWindowSurface[u8Wsrc] = pTmpS;
                IDirect3DTexture9* pTmpT = m_apTempVideoTexture[u8Vsrc];
                m_apTempVideoTexture[u8Vsrc] = m_apTempWindowTexture[u8Wsrc];
                m_apTempWindowTexture[u8Wsrc] = pTmpT;
            } else {// copy from mixer surface (an extremely rare case, as this stage can't really use unfiltered output from the mixer)
                EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->StretchRect(m_apVideoSurface[m_u8CurrentMixerSurface], nullptr, m_apTempWindowSurface[u8Wsrc], nullptr, D3DTEXF_POINT)));
            }
        }

        ++m_upSchedulerCorrectionCounter;
        if (unsigned __int8 u8SchedulerAdjustTimeOut = m_u8SchedulerAdjustTimeOut + 1) {// increment up to 255
            m_u8SchedulerAdjustTimeOut = u8SchedulerAdjustTimeOut;
        }
        ASSERT(m_u8SchedulerAdjustTimeOut);

        double dFrameStart = static_cast<double>(static_cast<ptrdiff_t>(m_upDetectedFrameTimePos)) * m_dDetectedVideoTimePerFrame;// convert the frame counter to time in seconds, the standard converter only does a proper job with signed values
        //double dFrameEnd = dFrameStart + m_dDetectedVideoTimePerFrame;
        m_afFTdividerILog[upCurrJitterPos] = static_cast<float>(m_dDetectedRefreshRate * m_dDetectedVideoTimePerFrame);// statistics
        double dFTdivider = m_dDetectedVideoFrameRate * m_dDetectedRefreshTime;
        double dScrOffs = dFrameStart * m_dDetectedRefreshRate;
        double dTRest = fmod(dScrOffs, 1.0) * dFTdivider;

        // calculate amount of inter-frames, implementation limit 8
        double dLimit = 1.0;
        if (!m_bSyncStatsAvailable) {// while the present mode is in force-sleep-delay-timing mode instead of immediate looping, dLimit has to be lowered to prevent delaying too much and cause frame dropping
            double dCompensate = m_dPaintTime * (1.0 / 16.0) + m_adDetectedFrameTimeHistory[upCurrJitterPos] - m_dDetectedVideoTimePerFrame;
            if (dCompensate < 0.0) {// only compensate by negative amounts
                dLimit += dCompensate * m_dDetectedVideoFrameRate * dFTdivider;
            }
        }

        // shader constant inputs
        // the 4-frame variant with random ordered dithering uses 19 sets
        // 3 unused sets that are written as excess in a loop
        // 1 set is used to store the inter-frame time data of pass 2 at the very end
        __declspec(align(16)) float afConstData[4 * (19 + 3 + 1)];
        /* layouts in memory, the last set is always used for storing the inter-frame time data of pass 2
        dither < 2:
        ts0, ts1, ts2, ts3

        dither >= 3:
        ts0, ts1, ts2, ts3,
        rnd, rnd, rnd, rnd

        dither == 2:
        1 frame:
        ts0, ts1, ts2, ts3,
        r0r, r0r, r0g, r0g,
        r0b, r0b, ign, ign,
        p0r, p0r, p0r, p0r,
        p0g, p0g, p0g, p0g,
        p0b, p0b, p0b, p0b

        2 frames:
        ts0, ts1, ts2, ts3,
        r0r, r0r, r0g, r0g,
        r0b, r0b, r1r, r1r,
        r1g, r1g, r1b, r1b,
        p0r, p0r, p0r, p0r,
        p0g, p0g, p0g, p0g,
        p0b, p0b, p0b, p0b,
        p1r, p1r, p1r, p1r,
        p1g, p1g, p1g, p1g,
        p1b, p1b, p1b, p1b

        3 frames:
        ts0, ts1, ts2, ts3,
        r0r, r0r, r0g, r0g,
        r0b, r0b, r1r, r1r,
        r1g, r1g, r1b, r1b,
        r2r, r2r, r2g, r2g,
        r2b, r2b, ign, ign,
        p0r, p0r, p0r, p0r,
        p0g, p0g, p0g, p0g,
        p0b, p0b, p0b, p0b,
        p1r, p1r, p1r, p1r,
        p1g, p1g, p1g, p1g,
        p1b, p1b, p1b, p1b,
        p2r, p2r, p2r, p2r,
        p2g, p2g, p2g, p2g,
        p2b, p2b, p2b, p2b

        4 frames:
        ts0, ts1, ts2, ts3,
        r0r, r0r, r0g, r0g,
        r0b, r0b, r1r, r1r,
        r1g, r1g, r1b, r1b,
        r2r, r2r, r2g, r2g,
        r2b, r2b, r3r, r3r,
        r3g, r3g, r3b, r3b,
        p0r, p0r, p0r, p0r,
        p0g, p0g, p0g, p0g,
        p0b, p0b, p0b, p0b,
        p1r, p1r, p1r, p1r,
        p1g, p1g, p1g, p1g,
        p1b, p1b, p1b, p1b,
        p2r, p2r, p2r, p2r,
        p2g, p2g, p2g, p2g,
        p2b, p2b, p2b, p2b,
        p3r, p3r, p3r, p3r,
        p3g, p3g, p3g, p3g,
        p3b, p3b, p3b, p3b */

        // put as many frames as possible into pass 1
        unsigned __int8 u8InterFrames = 1;
        afConstData[0] = static_cast<float>(dTRest);// note: even if dTRest would exceed dLimit, the method here requires that we produce at least 1 frame at all times
        unsigned __int8 i = 1;
        do {
            dTRest += dFTdivider;// calculate time fraction
            float fInterFrameTime = 1.0f;// limit to 1, as the present time correction mechanism might increment u8InterFrames afterwars regardless of the next check
            if (dTRest < dLimit) {
                fInterFrameTime = static_cast<float>(dTRest);
                ++u8InterFrames;// even if the cycle goes over the limit, the set of 8 is always completely initialized this way
            }
            afConstData[i] = fInterFrameTime;
            ++i;
        } while (i < 8);
        // copy the inter-frame time data of pass 2 to the end of afConstData, this will again be copied to the first 4 floats of the array in the second pass
        _mm_store_ps(afConstData + 4 * (19 + 3), _mm_load_ps(afConstData + 4));

        if (m_bSyncStatsAvailable && (m_upSchedulerCorrectionCounter > 5)) {// compensate a bit, this is a very rare event, compared to other schedulers
            double dOneAndAHalfTPF = 1.5 * m_dDetectedVideoTimePerFrame;
            double dTwoTPF = m_dDetectedVideoTimePerFrame + m_dDetectedVideoTimePerFrame;
            if ((dOneAndAHalfTPF < dSyncOffsetCurr) && (dOneAndAHalfTPF < dSyncOffsetAvrg)) {// force to high in case of drifting
                m_upSchedulerCorrectionCounter = 0;
                if (u8InterFrames < 8) {
                    ++u8InterFrames;
                }
                if ((dTwoTPF < dSyncOffsetCurr) && (dTwoTPF < dSyncOffsetAvrg)) {// two frame adaptation to correct large glitches
                    m_u8SchedulerAdjustTimeOut = 0;
                    if (u8InterFrames < 8) {
                        unsigned __int8 u8InterFramesOld = u8InterFrames;
                        ++u8InterFrames;
                        if (u8InterFramesOld < 7) {
                            dTwoTPF += m_dDetectedVideoTimePerFrame;
                            if ((dTwoTPF < dSyncOffsetCurr) && (dTwoTPF < dSyncOffsetAvrg)) {// three frame adaptation to correct huge glitches
                                ++u8InterFrames;
                                if (u8InterFramesOld < 6) {
                                    dTwoTPF += m_dDetectedVideoTimePerFrame;
                                    if ((dTwoTPF < dSyncOffsetCurr) && (dTwoTPF < dSyncOffsetAvrg)) {// four frame adaptation to correct gigantic glitches
                                        ++u8InterFrames;
                                        if (u8InterFramesOld < 5) {
                                            dTwoTPF += m_dDetectedVideoTimePerFrame;
                                            if ((dTwoTPF < dSyncOffsetCurr) && (dTwoTPF < dSyncOffsetAvrg)) {// five frame adaptation to correct colossal glitches
                                                ++u8InterFrames;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else if (m_u8SchedulerAdjustTimeOut == 255) {// adjust the detected refresh rate a bit when minor adjustments are made
                    m_dDetectedRefreshRate *= 1.0 + 1.0 / 8192.0;
                    m_dDetectedRefreshTime *= 1.0 / (1.0 + 1.0 / 8192.0);
                }
            } else if ((-dOneAndAHalfTPF > dSyncOffsetCurr) && (-dOneAndAHalfTPF > dSyncOffsetAvrg)) {// force to low in case of drifting
                m_upSchedulerCorrectionCounter = 0;
                if (u8InterFrames > 1) {
                    --u8InterFrames;
                }
                if ((-dTwoTPF > dSyncOffsetCurr) && (-dTwoTPF > dSyncOffsetAvrg)) {// two frame adaptation to correct large glitches
                    m_u8SchedulerAdjustTimeOut = 0;
                    if (u8InterFrames > 1) {
                        --u8InterFrames;
                    }
                } else if (m_u8SchedulerAdjustTimeOut == 255) {// adjust the detected refresh rate a bit when minor adjustments are made
                    m_dDetectedRefreshRate *= 1.0 / (1.0 + 1.0 / 8192.0);
                    m_dDetectedRefreshTime *= 1.0 + 1.0 / 8192.0;
                }
            }
        }
        ASSERT((u8InterFrames > 0) && (u8InterFrames < 9));
        m_au8PresentCountLog[upCurrJitterPos] = u8InterFrames;// statistics

        // create extra render target surfaces if required
        if (!m_pFIBufferRT0 && (u8InterFrames > 1)) {
            if (FAILED(hr = m_pD3DDev->CreateRenderTarget(m_dpPParam.BackBufferWidth, m_dpPParam.BackBufferHeight, m_dpPParam.BackBufferFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &m_pFIBufferRT0, nullptr))) {
                ErrBox(hr, L"creation of frame interpolation render target surface 0 failed\n");
            }
        }
        if (!m_pFIBufferRT1 && (u8InterFrames > 2)) {
            if (FAILED(hr = m_pD3DDev->CreateRenderTarget(m_dpPParam.BackBufferWidth, m_dpPParam.BackBufferHeight, m_dpPParam.BackBufferFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &m_pFIBufferRT1, nullptr))) {
                ErrBox(hr, L"creation of frame interpolation render target surface 1 failed\n");
            }
        }
        if (!m_pFIBufferRT2 && (u8InterFrames > 3)) {
            if (FAILED(hr = m_pD3DDev->CreateRenderTarget(m_dpPParam.BackBufferWidth, m_dpPParam.BackBufferHeight, m_dpPParam.BackBufferFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &m_pFIBufferRT2, nullptr))) {
                ErrBox(hr, L"creation of frame interpolation render target surface 2 failed\n");
            }
        }

        if (m_u8VMR9DitheringLevelsCurrent == 1 || m_u8VMR9DitheringLevelsCurrent == 2) {
            ASSERT(m_pDitherTexture);
            hr = m_pD3DDev->SetTexture(1, m_pDitherTexture);// set sampler: Dither
        }
        if (m_u8VMR9ColorManagementEnableCurrent == 1) {
            ASSERT(m_pLut3DTexture);
            hr = m_pD3DDev->SetTexture(2, m_pLut3DTexture);// set sampler: Lut3D
        }

        ASSERT(m_apTempWindowTexture[m_u8FIold]);
        hr = m_pD3DDev->SetTexture(0, m_apTempWindowTexture[m_u8FIold]);// old frame
        ASSERT(m_apTempWindowTexture[m_u8FIprevious]);
        hr = m_pD3DDev->SetTexture(5, m_apTempWindowTexture[m_u8FIprevious]);// previous frame
        ASSERT(m_apTempWindowTexture[m_u8FInext]);
        hr = m_pD3DDev->SetTexture(4, m_apTempWindowTexture[m_u8FInext]);// next frame
        ASSERT(m_apTempWindowTexture[u8Wsrc]);
        hr = m_pD3DDev->SetTexture(3, m_apTempWindowTexture[u8Wsrc]);// new frame

        if (m_u8VMR9FrameInterpolationCurrent != 1) {// pre-frame interpolation shading
            // motion estimation
            hr = m_pD3DDev->SetPixelShaderConstantF(0, m_afNormRectVideoArea, 1);// used to limit the interpolation to the active video area
            ASSERT(m_apFIPrePixelShader[0] && m_apFIPrePixelShader[1] && m_apFIPrePixelShader[2]);
            ASSERT(m_apFIPreSurface[m_u8FInext - 2]);
            hr = m_pD3DDev->SetRenderTarget(0, m_apFIPreSurface[m_u8FInext - 2]);
            hr = m_pD3DDev->SetPixelShader(m_apFIPrePixelShader[0]);
            hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 0

            ASSERT(m_apFIPreTexture[m_u8FInext - 2]);
            hr = m_pD3DDev->SetTexture(6, m_apFIPreTexture[m_u8FInext - 2]);

            // horizontal blur
            ASSERT(m_apFIPreSurface[3]);
            hr = m_pD3DDev->SetRenderTarget(0, m_apFIPreSurface[3]);
            hr = m_pD3DDev->SetPixelShader(m_apFIPrePixelShader[1]);
            hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 0

            ASSERT(m_apFIPreTexture[3]);
            hr = m_pD3DDev->SetTexture(6, m_apFIPreTexture[3]);

            // vertical blur
            hr = m_pD3DDev->SetRenderTarget(0, m_apFIPreSurface[m_u8FInext - 2]);
            hr = m_pD3DDev->SetPixelShader(m_apFIPrePixelShader[2]);
            hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 0

            ASSERT(m_apFIPreTexture[m_u8FIold - 2] && m_apFIPreTexture[m_u8FIprevious - 2]);
            hr = m_pD3DDev->SetTexture(6, m_apFIPreTexture[m_u8FIold - 2]);// old map
            hr = m_pD3DDev->SetTexture(7, m_apFIPreTexture[m_u8FIprevious - 2]);// previous map
            hr = m_pD3DDev->SetTexture(8, m_apFIPreTexture[m_u8FInext - 2]);// next map

            // linear inter-frame sampling
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(3, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(3, D3DSAMP_MINFILTER, D3DTEXF_LINEAR)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(4, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(4, D3DSAMP_MINFILTER, D3DTEXF_LINEAR)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(5, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(5, D3DSAMP_MINFILTER, D3DTEXF_LINEAR)));
        }

        // multiple render targets
        hr = m_pD3DDev->SetRenderTarget(0, m_pBackBuffer);
        if (u8InterFrames > 1) {
            ASSERT(m_pFIBufferRT0);
            hr = m_pD3DDev->SetRenderTarget(1, m_pFIBufferRT0);
            if (u8InterFrames > 2) {
                ASSERT(m_pFIBufferRT1);
                hr = m_pD3DDev->SetRenderTarget(2, m_pFIBufferRT1);
                if (u8InterFrames > 3) {
                    ASSERT(m_pFIBufferRT2);
                    hr = m_pD3DDev->SetRenderTarget(3, m_pFIBufferRT2);
                }
            }
        }

        unsigned __int8 u8Pass1ExtraFrames = u8InterFrames - 1;
        if (u8Pass1ExtraFrames > 3) {
            u8Pass1ExtraFrames = 3;
        }

        // set constants
        // 4 lookups combined: the first rand() count, the second rand() count, byte offset to place the projection data for sampling directions and the count of vectors in use for the activated shader
        static __declspec(align(4)) unsigned __int8 const aku8MultiLookup[16] = {
            1, 6 - 1, 3 * 16, 6,
            2, 12 - 1, 4 * 16, 10,
            2, 18 - 1, 6 * 16, 15,
            3, 24 - 1, 7 * 16, 19
        };

        UINT uiVector4fCount = 1;// default; dither levels 0 and 1, no additional input values
        if (m_u8VMR9DitheringLevelsCurrent > 1) {
            static_assert(RAND_MAX == 0x7FFF, "RAND_MAX changed, update these routines that currently depend on a 15-bit random system");
            // numbers for the sets, given for 4, 3, 2 and 1 frames of level 2, and level 3
            // vector counts: 19, 15, 10, 6, d 2
            // first set of interval [0, 1) random floats: 6*4, 6*3, 6*2, 6, d 4
            // second set of ±1, 0 generator vectors: 3*4, 3*3, 3*2, 3, d n/a
            // vector offset of second set: 7, 6, 4, 3, d n/a
            // set the defaults for random dither: 2 vectors in the end, 4 random numbers required for the next pass
            unsigned __int8 u8EndRCount;// used for the second loop
            ptrdiff_t ipRCount = 4 - 1;
            uintptr_t pDest;// used for the second loop
            uiVector4fCount = 2;

            // group calls together before the main part of the routine
            if (m_u8VMR9DitheringLevelsCurrent == 2) {// random ordered dither
                unsigned __int32 u32MultiLookup = reinterpret_cast<unsigned __int32 const*>(aku8MultiLookup)[u8Pass1ExtraFrames];
                u8EndRCount = static_cast<unsigned __int8>(u32MultiLookup);
                ipRCount = u32MultiLookup >> 8 & 0xFF;
                pDest = reinterpret_cast<uintptr_t>(afConstData) + (u32MultiLookup >> 16 & 0xFF);
                uiVector4fCount = u32MultiLookup >> 24;

                // each has 5 sets of 3 random bits
                // r0, g0, b0, r1, g1
                // b1, r2, g2, b2, r3
                // g3, b3, the unused 3 last items do take memory to write to in the loop of the second set
                // use the g3+2, g3+1 and g3 32-bit base offsets in the configuration for 4 frames to temporarily store the rand() output, so that these do not get overwritten until the last pass of the loop
                unsigned __int8 u8EndRCountCopy = u8EndRCount;
                do {
                    reinterpret_cast<__int32*>(afConstData + 4 * 17 - 1)[u8EndRCountCopy] = rand();
                } while (--u8EndRCountCopy);
            }

            do {// fill with temporary random integers
                reinterpret_cast<__int32*>(afConstData + 4)[ipRCount] = rand() << 8;// shifted left: lined up to fit the 23 mantissa bits of floats
            } while (--ipRCount >= 0);

            // insert at least the 4 random floats required for both dithering types
            // if some of the upper numbers are not yet initialized here, it doesn't matter, this part of the code is practically only slowed down by the first load operation
            // [0, 1) random generator
            static __declspec(align(16)) float const akfOne[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            __m128 xOne = _mm_load_ps(akfOne);
            __m128 xRnd0 = _mm_load_ps(afConstData + 4);
            __m128 xRnd1 = _mm_load_ps(afConstData + 8);
            __m128 xRnd2 = _mm_load_ps(afConstData + 12);
            __m128 xRnd3 = _mm_load_ps(afConstData + 16);
            __m128 xRnd4 = _mm_load_ps(afConstData + 20);
            __m128 xRnd5 = _mm_load_ps(afConstData + 24);
            xRnd0 = _mm_or_ps(xRnd0, xOne);// note that 1.0f has all mantissa bits 0
            xRnd1 = _mm_or_ps(xRnd1, xOne);
            xRnd2 = _mm_or_ps(xRnd2, xOne);
            xRnd3 = _mm_or_ps(xRnd3, xOne);
            xRnd4 = _mm_or_ps(xRnd4, xOne);
            xRnd5 = _mm_or_ps(xRnd5, xOne);
            xRnd0 = _mm_sub_ps(xRnd0, xOne);// shift the numbers from interval [1, 2) to [0, 1)
            xRnd1 = _mm_sub_ps(xRnd1, xOne);
            xRnd2 = _mm_sub_ps(xRnd2, xOne);
            xRnd3 = _mm_sub_ps(xRnd3, xOne);
            xRnd4 = _mm_sub_ps(xRnd4, xOne);
            xRnd5 = _mm_sub_ps(xRnd5, xOne);
            _mm_store_ps(afConstData + 4, xRnd0);
            _mm_store_ps(afConstData + 8, xRnd1);
            _mm_store_ps(afConstData + 12, xRnd2);
            _mm_store_ps(afConstData + 16, xRnd3);
            _mm_store_ps(afConstData + 20, xRnd4);
            _mm_store_ps(afConstData + 24, xRnd5);

            if (m_u8VMR9DitheringLevelsCurrent == 2) {
                // ±1, 0 projection data for sampling directions generator
                // 8 configurations, made using 3 random bits:
                // a = ( 1,  1), b = ( 0,  0)
                // a = ( 1, -1), b = ( 0,  0)
                // a = (-1,  1), b = ( 0,  0)
                // a = (-1, -1), b = ( 0,  0)
                // a = ( 0,  0), b = ( 1,  1)
                // a = ( 0,  0), b = ( 1, -1)
                // a = ( 0,  0), b = (-1,  1)
                // a = ( 0,  0), b = (-1, -1)
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
                static __declspec(align(16)) __int32 const aki32SignBits[4] = {0x80000000, 0x80000000, 0x80000000, 0x80000000};
                __m128i xSignBits = _mm_load_si128(reinterpret_cast<__m128i const*>(aki32SignBits));
                do {
                    __m128i xBits0123 = _mm_cvtsi32_si128(reinterpret_cast<__int32*>(afConstData + 4 * 17 - 1)[u8EndRCount]);// correctly compiles to movd xmmr, m32
                    __m128i xBits89AB = xBits0123;
                    xBits0123 = _mm_slli_epi64(xBits0123, 33);
                    xBits89AB = _mm_or_si128(xBits89AB, xBits0123);
                    __m128i xBitsCDE = xBits89AB;
                    xBits89AB = _mm_slli_epi64(xBits89AB, 2);
                    xBitsCDE = _mm_unpacklo_epi64(xBitsCDE, xBits89AB);// result: packed vector, the input 32-bit integer is left-shifted by 0, 1, 2 and 3 places
                    // left-shift the numbers to get the random bits to bit 31
                    __m128i xBits4567 = xBitsCDE;
                    xBits0123 = xBitsCDE;
                    xBits89AB = xBitsCDE;
                    xBitsCDE = _mm_slli_si128(xBitsCDE, 2);// n, 17, 18, 19
                    xBits4567 = _mm_slli_si128(xBits4567, 3);// 24, 25, 26, 27
                    xBits0123 = _mm_slli_epi64(xBits0123, 28);// 28, 29, 30, 31
                    xBits89AB = _mm_slli_epi64(xBits89AB, 20);// 20, 21, 22, 23
                    // create lower or higher bitmasks of the first set
                    __m128i xBits4567c = xBits4567;
                    xBitsCDE = _mm_srai_epi32(xBitsCDE, 31);
                    xBits4567 = _mm_srai_epi32(xBits4567, 31);
                    // keep only the sign bits of the second set
                    xBits4567c = _mm_and_si128(xBits4567c, xSignBits);
                    xBits0123 = _mm_and_si128(xBits0123, xSignBits);
                    xBits89AB = _mm_and_si128(xBits89AB, xSignBits);
                    // expand the bitmasks of the first set to 64 bits each
                    __m128i xBitsM = _mm_shuffle_epi32(xBitsCDE, _MM_SHUFFLE(3, 2, 1, 1));
                    xBitsCDE = _mm_shuffle_epi32(xBitsCDE, _MM_SHUFFLE(3, 3, 2, 2));
                    xBits4567 = _mm_shuffle_epi32(xBits4567, _MM_SHUFFLE(3, 3, 2, 2));
                    // apply the sign bits of the second set on 1.0f
                    xBits4567c = _mm_or_si128(xBits4567c, _mm_castps_si128(xOne));
                    xBits0123 = _mm_or_si128(xBits0123, _mm_castps_si128(xOne));
                    xBits89AB = _mm_or_si128(xBits89AB, _mm_castps_si128(xOne));
                    // combine the sets to create the finished values
                    __m128i xBitsMn = xBitsM;
                    __m128i xBitsCDEn = xBitsCDE;
                    __m128i xBits4567n = xBits4567;
                    xBitsM = _mm_and_si128(xBitsM, xBits4567c);
                    xBitsCDE = _mm_and_si128(xBitsCDE, xBits0123);
                    xBits4567 = _mm_and_si128(xBits4567, xBits89AB);
                    xBitsMn = _mm_andnot_si128(xBitsMn, xBits4567c);
                    xBitsCDEn = _mm_andnot_si128(xBitsCDEn, xBits0123);
                    xBits4567n = _mm_andnot_si128(xBits4567n, xBits89AB);
                    // combine upper and lower parts of each set
                    __m128i xBitsCDEt = xBitsCDE;
                    __m128i xBits4567t = xBits4567;
                    xBitsM = _mm_unpacklo_epi64(xBitsM, xBitsMn);
                    xBitsCDE = _mm_unpacklo_epi64(xBitsCDE, xBitsCDEn);
                    xBits4567 = _mm_unpacklo_epi64(xBits4567, xBits4567n);
                    xBitsCDEt = _mm_unpackhi_epi64(xBitsCDEt, xBitsCDEn);
                    xBits4567 = _mm_unpackhi_epi64(xBits4567t, xBits4567n);
                    // store five sets
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDest), xBitsM);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDest + 16), xBitsCDE);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDest + 32), xBits4567);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDest + 48), xBitsCDEt);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDest + 64), xBits4567t);
                    pDest += 80;
                } while (--u8EndRCount);
#else
                __int32 i32One = 0x3F800000, i32SignBit = 0x80000000;// 0x3F800000 is 1.0f
                do {
                    __int32 i32Rn = *reinterpret_cast<__int32*>(afConstData + 4 * (19 + 3) - u8EndRCount);
                    // left-shift the numbers to get the random bits to bit 31 and apply it on one
                    __int32 i32rSignLow1 = (i32Rn << 28 & i32SignBit) | i32One;// bit 3
                    __int32 i32rSignLow2 = (i32Rn << 25 & i32SignBit) | i32One;// bit 6
                    __int32 i32rSignLow3 = (i32Rn << 22 & i32SignBit) | i32One;// bit 9
                    __int32 i32rSignLow4 = (i32Rn << 19 & i32SignBit) | i32One;// bit 12
                    __int32 i32rSignLow0 = i32Rn << 31 | i32One;// bit 0, no masking required
                    __int32 i32rSignHigh0 = (i32Rn << 30 & i32SignBit) | i32One;// bit 1 to bit 31
                    __int32 i32rSignHigh1 = (i32Rn << 27 & i32SignBit) | i32One;// bit 4
                    __int32 i32rSignHigh2 = (i32Rn << 24 & i32SignBit) | i32One;// bit 7
                    __int32 i32rSignHigh3 = (i32Rn << 21 & i32SignBit) | i32One;// bit 10
                    __int32 i32rSignHigh4 = (i32Rn << 18 & i32SignBit) | i32One;// bit 13
                    // create lower bitmasks, the high bitmasks are just the bitwise negation
                    __int32 i32rLowMask0 = (i32Rn << 29) >> 31;// bit 2
                    __int32 i32rLowMask1 = (i32Rn << 26) >> 31;// bit 5
                    __int32 i32rLowMask2 = (i32Rn << 23) >> 31;// bit 8
                    __int32 i32rLowMask3 = (i32Rn << 20) >> 31;// bit 11
                    __int32 i32rLowMask4 = (i32Rn << 17) >> 31;// bit 14
                    // apply the masks and store five sets
                    reinterpret_cast<__int32*>(pDest)[0] = i32rSignLow0 & i32rLowMask0;
                    reinterpret_cast<__int32*>(pDest)[1] = i32rSignHigh0 & i32rLowMask0;
                    reinterpret_cast<__int32*>(pDest)[2] = i32rSignLow0 & ~i32rLowMask0;
                    reinterpret_cast<__int32*>(pDest)[3] = i32rSignHigh0 & ~i32rLowMask0;
                    reinterpret_cast<__int32*>(pDest)[4] = i32rSignLow1 & i32rLowMask1;
                    reinterpret_cast<__int32*>(pDest)[5] = i32rSignHigh1 & i32rLowMask1;
                    reinterpret_cast<__int32*>(pDest)[6] = i32rSignLow1 & ~i32rLowMask1;
                    reinterpret_cast<__int32*>(pDest)[7] = i32rSignHigh1 & ~i32rLowMask1;
                    reinterpret_cast<__int32*>(pDest)[8] = i32rSignLow2 & i32rLowMask2;
                    reinterpret_cast<__int32*>(pDest)[9] = i32rSignHigh2 & i32rLowMask2;
                    reinterpret_cast<__int32*>(pDest)[10] = i32rSignLow2 & ~i32rLowMask2;
                    reinterpret_cast<__int32*>(pDest)[11] = i32rSignHigh2 & ~i32rLowMask2;
                    reinterpret_cast<__int32*>(pDest)[12] = i32rSignLow3 & i32rLowMask3;
                    reinterpret_cast<__int32*>(pDest)[13] = i32rSignHigh3 & i32rLowMask3;
                    reinterpret_cast<__int32*>(pDest)[14] = i32rSignLow3 & ~i32rLowMask3;
                    reinterpret_cast<__int32*>(pDest)[15] = i32rSignHigh3 & ~i32rLowMask3;
                    reinterpret_cast<__int32*>(pDest)[16] = i32rSignLow4 & i32rLowMask4;
                    reinterpret_cast<__int32*>(pDest)[17] = i32rSignHigh4 & i32rLowMask4;
                    reinterpret_cast<__int32*>(pDest)[18] = i32rSignLow4 & ~i32rLowMask4;
                    reinterpret_cast<__int32*>(pDest)[19] = i32rSignHigh4 & ~i32rLowMask4;
                    pDest += 80;
                } while (--u8EndRCount);
#endif
            }
        }
        hr = m_pD3DDev->SetPixelShaderConstantF(0, afConstData, uiVector4fCount);
        ASSERT(m_apFIPixelShader[u8Pass1ExtraFrames]);
        hr = m_pD3DDev->SetPixelShader(m_apFIPixelShader[u8Pass1ExtraFrames]);
        hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 0

        // release render targets and present, spare one inter frame to present at the next Paint() cycle
        if (m_pRenderersData->m_fTearingTest && (m_dpPParam.BackBufferWidth > 22)) {
            if (m_rcTearing.left + 23 > static_cast<LONG>(m_dpPParam.BackBufferWidth)) {
                m_rcTearing.left = 0;
            }
            m_rcTearing.right = m_rcTearing.left + 4;
            m_pD3DDev->ColorFill(m_pBackBuffer, &m_rcTearing, 0xFFFF0000);
            m_rcTearing.left += 19;
            m_rcTearing.right = m_rcTearing.left + 4;
            m_pD3DDev->ColorFill(m_pBackBuffer, &m_rcTearing, 0xFFFF0000);
            m_rcTearing.left += 7;
        }
        if (u8InterFrames > 1) {
            m_pD3DDev->SetRenderTarget(0, m_apVideoSurface[m_u8CurrentMixerSurface]);// index 0 may never point to nullptr and may not point to the backbuffer when calling a present
            if (bPresentEx) {
                m_pD3DDev->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
            } else {
                m_pSwapChain->Present(nullptr, nullptr, nullptr, nullptr, 0);
            }
            m_pD3DDev->SetRenderTarget(1, nullptr);

            if (m_pRenderersData->m_fTearingTest && (m_dpPParam.BackBufferWidth > 22)) {
                if (m_rcTearing.left + 23 > static_cast<LONG>(m_dpPParam.BackBufferWidth)) {
                    m_rcTearing.left = 0;
                }
                m_rcTearing.right = m_rcTearing.left + 4;
                m_pD3DDev->ColorFill(m_pFIBufferRT0, &m_rcTearing, 0xFFFF0000);
                m_rcTearing.left += 19;
                m_rcTearing.right = m_rcTearing.left + 4;
                m_pD3DDev->ColorFill(m_pFIBufferRT0, &m_rcTearing, 0xFFFF0000);
                m_rcTearing.left += 7;
            }
            hr = m_pD3DDev->StretchRect(m_pFIBufferRT0, nullptr, m_pBackBuffer, nullptr, D3DTEXF_POINT);
            if (u8InterFrames > 2) {
                if (bPresentEx) {
                    m_pD3DDev->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
                } else {
                    m_pSwapChain->Present(nullptr, nullptr, nullptr, nullptr, 0);
                }
                m_pD3DDev->SetRenderTarget(2, nullptr);

                if (m_pRenderersData->m_fTearingTest && (m_dpPParam.BackBufferWidth > 22)) {
                    if (m_rcTearing.left + 23 > static_cast<LONG>(m_dpPParam.BackBufferWidth)) {
                        m_rcTearing.left = 0;
                    }
                    m_rcTearing.right = m_rcTearing.left + 4;
                    m_pD3DDev->ColorFill(m_pFIBufferRT1, &m_rcTearing, 0xFFFF0000);
                    m_rcTearing.left += 19;
                    m_rcTearing.right = m_rcTearing.left + 4;
                    m_pD3DDev->ColorFill(m_pFIBufferRT1, &m_rcTearing, 0xFFFF0000);
                    m_rcTearing.left += 7;
                }
                hr = m_pD3DDev->StretchRect(m_pFIBufferRT1, nullptr, m_pBackBuffer, nullptr, D3DTEXF_POINT);
                if (u8InterFrames > 3) {
                    if (bPresentEx) {
                        m_pD3DDev->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
                    } else {
                        m_pSwapChain->Present(nullptr, nullptr, nullptr, nullptr, 0);
                    }
                    m_pD3DDev->SetRenderTarget(3, nullptr);

                    if (m_pRenderersData->m_fTearingTest && (m_dpPParam.BackBufferWidth > 22)) {
                        if (m_rcTearing.left + 23 > static_cast<LONG>(m_dpPParam.BackBufferWidth)) {
                            m_rcTearing.left = 0;
                        }
                        m_rcTearing.right = m_rcTearing.left + 4;
                        m_pD3DDev->ColorFill(m_pFIBufferRT2, &m_rcTearing, 0xFFFF0000);
                        m_rcTearing.left += 19;
                        m_rcTearing.right = m_rcTearing.left + 4;
                        m_pD3DDev->ColorFill(m_pFIBufferRT2, &m_rcTearing, 0xFFFF0000);
                        m_rcTearing.left += 7;
                    }
                    hr = m_pD3DDev->StretchRect(m_pFIBufferRT2, nullptr, m_pBackBuffer, nullptr, D3DTEXF_POINT);

                    if (u8InterFrames > 4) {
                        if (bPresentEx) {
                            m_pD3DDev->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
                        } else {
                            m_pSwapChain->Present(nullptr, nullptr, nullptr, nullptr, 0);
                        }
                    }
                }
            }
        }

        if (u8InterFrames > 4) {// do a second pass
            // multiple render targets
            hr = m_pD3DDev->SetRenderTarget(0, m_pBackBuffer);
            if (u8InterFrames > 5) {
                ASSERT(m_pFIBufferRT0);
                hr = m_pD3DDev->SetRenderTarget(1, m_pFIBufferRT0);
                if (u8InterFrames > 6) {
                    ASSERT(m_pFIBufferRT1);
                    hr = m_pD3DDev->SetRenderTarget(2, m_pFIBufferRT1);
                    if (u8InterFrames > 7) {
                        ASSERT(m_pFIBufferRT2);
                        hr = m_pD3DDev->SetRenderTarget(3, m_pFIBufferRT2);
                    }
                }
            }

            unsigned __int8 u8Pass2ExtraFrames = u8InterFrames - 1 - 4;

            // set constants
            // copy the inter-frame time data of pass 2 from the end to the first 4 floats
            _mm_store_ps(afConstData, _mm_load_ps(afConstData + 4 * (19 + 3)));

            // uiVector4fCount = 1; default; dither levels 0 and 1, no additional input values, this should already have been set by the first pass
            ASSERT((m_u8VMR9DitheringLevelsCurrent > 1) || (uiVector4fCount == 1));
            if (m_u8VMR9DitheringLevelsCurrent > 1) {
                static_assert(RAND_MAX == 0x7FFF, "RAND_MAX changed, update these routines that currently depend on a 15-bit random system");
                // numbers for the sets, given for 4, 3, 2 and 1 frames of level 2, and level 3
                // vector counts: 19, 15, 10, 6, d 2
                // first set of interval [0, 1) random floats: 6*4, 6*3, 6*2, 6, d 4
                // second set of ±1, 0 generator vectors: 3*4, 3*3, 3*2, 3, d n/a
                // vector offset of second set: 7, 6, 4, 3, d n/a
                // set the defaults for random dither: 2 vectors in the end, 4 random numbers required for the next pass
                unsigned __int8 u8EndRCount;// used for the second loop
                ptrdiff_t ipRCount = 4 - 1;
                uintptr_t pDest;// used for the second loop
                // uiVector4fCount = 2; this should already have been set by the first pass
                ASSERT((m_u8VMR9DitheringLevelsCurrent == 2) || (uiVector4fCount == 2));

                // group calls together before the main part of the routine
                if (m_u8VMR9DitheringLevelsCurrent == 2) {// random ordered dither
                    unsigned __int32 u32MultiLookup = reinterpret_cast<unsigned __int32 const*>(aku8MultiLookup)[u8Pass2ExtraFrames];
                    u8EndRCount = static_cast<unsigned __int8>(u32MultiLookup);
                    ipRCount = u32MultiLookup >> 8 & 0xFF;
                    pDest = reinterpret_cast<uintptr_t>(afConstData) + (u32MultiLookup >> 16 & 0xFF);
                    uiVector4fCount = u32MultiLookup >> 24;

                    // each has 5 sets of 3 random bits
                    // r0, g0, b0, r1, g1
                    // b1, r2, g2, b2, r3
                    // g3, b3, the unused 3 last items do take memory to write to in the loop of the second set
                    // use the g3+2, g3+1 and g3 32-bit base offsets in the configuration for 4 frames to temporarily store the rand() output, so that these do not get overwritten until the last pass of the loop
                    unsigned __int8 u8EndRCountCopy = u8EndRCount;
                    do {
                        reinterpret_cast<__int32*>(afConstData + 4 * 17 - 1)[u8EndRCountCopy] = rand();
                    } while (--u8EndRCountCopy);
                }

                do {// fill with temporary random integers
                    reinterpret_cast<__int32*>(afConstData + 4)[ipRCount] = rand() << 8;// shifted left: lined up to fit the 23 mantissa bits of floats
                } while (--ipRCount >= 0);

                // insert at least the 4 random floats required for both dithering types
                // if some of the upper numbers are not yet initialized here, it doesn't matter, this part of the code is practically only slowed down by the first load operation
                // [0, 1) random generator
                static __declspec(align(16)) float const akfOne[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                __m128 xOne = _mm_load_ps(akfOne);
                __m128 xRnd0 = _mm_load_ps(afConstData + 4);
                __m128 xRnd1 = _mm_load_ps(afConstData + 8);
                __m128 xRnd2 = _mm_load_ps(afConstData + 12);
                __m128 xRnd3 = _mm_load_ps(afConstData + 16);
                __m128 xRnd4 = _mm_load_ps(afConstData + 20);
                __m128 xRnd5 = _mm_load_ps(afConstData + 24);
                xRnd0 = _mm_or_ps(xRnd0, xOne);// note that 1.0f has all mantissa bits 0
                xRnd1 = _mm_or_ps(xRnd1, xOne);
                xRnd2 = _mm_or_ps(xRnd2, xOne);
                xRnd3 = _mm_or_ps(xRnd3, xOne);
                xRnd4 = _mm_or_ps(xRnd4, xOne);
                xRnd5 = _mm_or_ps(xRnd5, xOne);
                xRnd0 = _mm_sub_ps(xRnd0, xOne);// shift the numbers from interval [1, 2) to [0, 1)
                xRnd1 = _mm_sub_ps(xRnd1, xOne);
                xRnd2 = _mm_sub_ps(xRnd2, xOne);
                xRnd3 = _mm_sub_ps(xRnd3, xOne);
                xRnd4 = _mm_sub_ps(xRnd4, xOne);
                xRnd5 = _mm_sub_ps(xRnd5, xOne);
                _mm_store_ps(afConstData + 4, xRnd0);
                _mm_store_ps(afConstData + 8, xRnd1);
                _mm_store_ps(afConstData + 12, xRnd2);
                _mm_store_ps(afConstData + 16, xRnd3);
                _mm_store_ps(afConstData + 20, xRnd4);
                _mm_store_ps(afConstData + 24, xRnd5);

                if (m_u8VMR9DitheringLevelsCurrent == 2) {
                    // ±1, 0 projection data for sampling directions generator
                    // 8 configurations, made using 3 random bits:
                    // a = ( 1,  1), b = ( 0,  0)
                    // a = ( 1, -1), b = ( 0,  0)
                    // a = (-1,  1), b = ( 0,  0)
                    // a = (-1, -1), b = ( 0,  0)
                    // a = ( 0,  0), b = ( 1,  1)
                    // a = ( 0,  0), b = ( 1, -1)
                    // a = ( 0,  0), b = (-1,  1)
                    // a = ( 0,  0), b = (-1, -1)
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
                    static __declspec(align(16)) __int32 const aki32SignBits[4] = {0x80000000, 0x80000000, 0x80000000, 0x80000000};
                    __m128i xSignBits = _mm_load_si128(reinterpret_cast<__m128i const*>(aki32SignBits));
                    do {
                        __m128i xBits0123 = _mm_cvtsi32_si128(reinterpret_cast<__int32*>(afConstData + 4 * 17 - 1)[u8EndRCount]);// correctly compiles to movd xmmr, m32
                        __m128i xBits89AB = xBits0123;
                        xBits0123 = _mm_slli_epi64(xBits0123, 33);
                        xBits89AB = _mm_or_si128(xBits89AB, xBits0123);
                        __m128i xBitsCDE = xBits89AB;
                        xBits89AB = _mm_slli_epi64(xBits89AB, 2);
                        xBitsCDE = _mm_unpacklo_epi64(xBitsCDE, xBits89AB);// result: packed vector, the input 32-bit integer is left-shifted by 0, 1, 2 and 3 places
                        // left-shift the numbers to get the random bits to bit 31
                        __m128i xBits4567 = xBitsCDE;
                        xBits0123 = xBitsCDE;
                        xBits89AB = xBitsCDE;
                        xBitsCDE = _mm_slli_si128(xBitsCDE, 2);// n, 17, 18, 19
                        xBits4567 = _mm_slli_si128(xBits4567, 3);// 24, 25, 26, 27
                        xBits0123 = _mm_slli_epi64(xBits0123, 28);// 28, 29, 30, 31
                        xBits89AB = _mm_slli_epi64(xBits89AB, 20);// 20, 21, 22, 23
                        // create lower or higher bitmasks of the first set
                        __m128i xBits4567c = xBits4567;
                        xBitsCDE = _mm_srai_epi32(xBitsCDE, 31);
                        xBits4567 = _mm_srai_epi32(xBits4567, 31);
                        // keep only the sign bits of the second set
                        xBits4567c = _mm_and_si128(xBits4567c, xSignBits);
                        xBits0123 = _mm_and_si128(xBits0123, xSignBits);
                        xBits89AB = _mm_and_si128(xBits89AB, xSignBits);
                        // expand the bitmasks of the first set to 64 bits each
                        __m128i xBitsM = _mm_shuffle_epi32(xBitsCDE, _MM_SHUFFLE(3, 2, 1, 1));
                        xBitsCDE = _mm_shuffle_epi32(xBitsCDE, _MM_SHUFFLE(3, 3, 2, 2));
                        xBits4567 = _mm_shuffle_epi32(xBits4567, _MM_SHUFFLE(3, 3, 2, 2));
                        // apply the sign bits of the second set on 1.0f
                        xBits4567c = _mm_or_si128(xBits4567c, _mm_castps_si128(xOne));
                        xBits0123 = _mm_or_si128(xBits0123, _mm_castps_si128(xOne));
                        xBits89AB = _mm_or_si128(xBits89AB, _mm_castps_si128(xOne));
                        // combine the sets to create the finished values
                        __m128i xBitsMn = xBitsM;
                        __m128i xBitsCDEn = xBitsCDE;
                        __m128i xBits4567n = xBits4567;
                        xBitsM = _mm_and_si128(xBitsM, xBits4567c);
                        xBitsCDE = _mm_and_si128(xBitsCDE, xBits0123);
                        xBits4567 = _mm_and_si128(xBits4567, xBits89AB);
                        xBitsMn = _mm_andnot_si128(xBitsMn, xBits4567c);
                        xBitsCDEn = _mm_andnot_si128(xBitsCDEn, xBits0123);
                        xBits4567n = _mm_andnot_si128(xBits4567n, xBits89AB);
                        // combine upper and lower parts of each set
                        __m128i xBitsCDEt = xBitsCDE;
                        __m128i xBits4567t = xBits4567;
                        xBitsM = _mm_unpacklo_epi64(xBitsM, xBitsMn);
                        xBitsCDE = _mm_unpacklo_epi64(xBitsCDE, xBitsCDEn);
                        xBits4567 = _mm_unpacklo_epi64(xBits4567, xBits4567n);
                        xBitsCDEt = _mm_unpackhi_epi64(xBitsCDEt, xBitsCDEn);
                        xBits4567 = _mm_unpackhi_epi64(xBits4567t, xBits4567n);
                        // store five sets
                        _mm_store_si128(reinterpret_cast<__m128i*>(pDest), xBitsM);
                        _mm_store_si128(reinterpret_cast<__m128i*>(pDest + 16), xBitsCDE);
                        _mm_store_si128(reinterpret_cast<__m128i*>(pDest + 32), xBits4567);
                        _mm_store_si128(reinterpret_cast<__m128i*>(pDest + 48), xBitsCDEt);
                        _mm_store_si128(reinterpret_cast<__m128i*>(pDest + 64), xBits4567t);
                        pDest += 80;
                    } while (--u8EndRCount);
#else
                    __int32 i32One = 0x3F800000, i32SignBit = 0x80000000;// 0x3F800000 is 1.0f
                    do {
                        __int32 i32Rn = *reinterpret_cast<__int32*>(afConstData + 4 * (19 + 3) - u8EndRCount);
                        // left-shift the numbers to get the random bits to bit 31 and apply it on one
                        __int32 i32rSignLow1 = (i32Rn << 28 & i32SignBit) | i32One;// bit 3
                        __int32 i32rSignLow2 = (i32Rn << 25 & i32SignBit) | i32One;// bit 6
                        __int32 i32rSignLow3 = (i32Rn << 22 & i32SignBit) | i32One;// bit 9
                        __int32 i32rSignLow4 = (i32Rn << 19 & i32SignBit) | i32One;// bit 12
                        __int32 i32rSignLow0 = i32Rn << 31 | i32One;// bit 0, no masking required
                        __int32 i32rSignHigh0 = (i32Rn << 30 & i32SignBit) | i32One;// bit 1 to bit 31
                        __int32 i32rSignHigh1 = (i32Rn << 27 & i32SignBit) | i32One;// bit 4
                        __int32 i32rSignHigh2 = (i32Rn << 24 & i32SignBit) | i32One;// bit 7
                        __int32 i32rSignHigh3 = (i32Rn << 21 & i32SignBit) | i32One;// bit 10
                        __int32 i32rSignHigh4 = (i32Rn << 18 & i32SignBit) | i32One;// bit 13
                        // create lower bitmasks, the high bitmasks are just the bitwise negation
                        __int32 i32rLowMask0 = (i32Rn << 29) >> 31;// bit 2
                        __int32 i32rLowMask1 = (i32Rn << 26) >> 31;// bit 5
                        __int32 i32rLowMask2 = (i32Rn << 23) >> 31;// bit 8
                        __int32 i32rLowMask3 = (i32Rn << 20) >> 31;// bit 11
                        __int32 i32rLowMask4 = (i32Rn << 17) >> 31;// bit 14
                        // apply the masks and store five sets
                        reinterpret_cast<__int32*>(pDest)[0] = i32rSignLow0 & i32rLowMask0;
                        reinterpret_cast<__int32*>(pDest)[1] = i32rSignHigh0 & i32rLowMask0;
                        reinterpret_cast<__int32*>(pDest)[2] = i32rSignLow0 & ~i32rLowMask0;
                        reinterpret_cast<__int32*>(pDest)[3] = i32rSignHigh0 & ~i32rLowMask0;
                        reinterpret_cast<__int32*>(pDest)[4] = i32rSignLow1 & i32rLowMask1;
                        reinterpret_cast<__int32*>(pDest)[5] = i32rSignHigh1 & i32rLowMask1;
                        reinterpret_cast<__int32*>(pDest)[6] = i32rSignLow1 & ~i32rLowMask1;
                        reinterpret_cast<__int32*>(pDest)[7] = i32rSignHigh1 & ~i32rLowMask1;
                        reinterpret_cast<__int32*>(pDest)[8] = i32rSignLow2 & i32rLowMask2;
                        reinterpret_cast<__int32*>(pDest)[9] = i32rSignHigh2 & i32rLowMask2;
                        reinterpret_cast<__int32*>(pDest)[10] = i32rSignLow2 & ~i32rLowMask2;
                        reinterpret_cast<__int32*>(pDest)[11] = i32rSignHigh2 & ~i32rLowMask2;
                        reinterpret_cast<__int32*>(pDest)[12] = i32rSignLow3 & i32rLowMask3;
                        reinterpret_cast<__int32*>(pDest)[13] = i32rSignHigh3 & i32rLowMask3;
                        reinterpret_cast<__int32*>(pDest)[14] = i32rSignLow3 & ~i32rLowMask3;
                        reinterpret_cast<__int32*>(pDest)[15] = i32rSignHigh3 & ~i32rLowMask3;
                        reinterpret_cast<__int32*>(pDest)[16] = i32rSignLow4 & i32rLowMask4;
                        reinterpret_cast<__int32*>(pDest)[17] = i32rSignHigh4 & i32rLowMask4;
                        reinterpret_cast<__int32*>(pDest)[18] = i32rSignLow4 & ~i32rLowMask4;
                        reinterpret_cast<__int32*>(pDest)[19] = i32rSignHigh4 & ~i32rLowMask4;
                        pDest += 80;
                    } while (--u8EndRCount);
#endif
                }
            }
            hr = m_pD3DDev->SetPixelShaderConstantF(0, afConstData, uiVector4fCount);
            if (u8Pass2ExtraFrames != 3) {// re-use the previously set 4-rendertarget output pixel shader if possible
                ASSERT(m_apFIPixelShader[u8Pass2ExtraFrames]);
                hr = m_pD3DDev->SetPixelShader(m_apFIPixelShader[u8Pass2ExtraFrames]);
            }
            hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 0

            // release render targets and present, spare one frame of uPass2InterFrames
            if (m_pRenderersData->m_fTearingTest && (m_dpPParam.BackBufferWidth > 22)) {
                if (m_rcTearing.left + 23 > static_cast<LONG>(m_dpPParam.BackBufferWidth)) {
                    m_rcTearing.left = 0;
                }
                m_rcTearing.right = m_rcTearing.left + 4;
                m_pD3DDev->ColorFill(m_pBackBuffer, &m_rcTearing, 0xFFFF0000);
                m_rcTearing.left += 19;
                m_rcTearing.right = m_rcTearing.left + 4;
                m_pD3DDev->ColorFill(m_pBackBuffer, &m_rcTearing, 0xFFFF0000);
                m_rcTearing.left += 7;
            }
            if (u8InterFrames > 5) {
                m_pD3DDev->SetRenderTarget(0, m_apVideoSurface[m_u8CurrentMixerSurface]);// index 0 may never point to nullptr and may not point to the backbuffer when calling a present
                if (bPresentEx) {
                    m_pD3DDev->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
                } else {
                    m_pSwapChain->Present(nullptr, nullptr, nullptr, nullptr, 0);
                }
                m_pD3DDev->SetRenderTarget(1, nullptr);

                if (m_pRenderersData->m_fTearingTest && (m_dpPParam.BackBufferWidth > 22)) {
                    if (m_rcTearing.left + 23 > static_cast<LONG>(m_dpPParam.BackBufferWidth)) {
                        m_rcTearing.left = 0;
                    }
                    m_rcTearing.right = m_rcTearing.left + 4;
                    m_pD3DDev->ColorFill(m_pFIBufferRT0, &m_rcTearing, 0xFFFF0000);
                    m_rcTearing.left += 19;
                    m_rcTearing.right = m_rcTearing.left + 4;
                    m_pD3DDev->ColorFill(m_pFIBufferRT0, &m_rcTearing, 0xFFFF0000);
                    m_rcTearing.left += 7;
                }
                hr = m_pD3DDev->StretchRect(m_pFIBufferRT0, nullptr, m_pBackBuffer, nullptr, D3DTEXF_POINT);
                if (u8InterFrames > 6) {
                    if (bPresentEx) {
                        m_pD3DDev->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
                    } else {
                        m_pSwapChain->Present(nullptr, nullptr, nullptr, nullptr, 0);
                    }
                    m_pD3DDev->SetRenderTarget(2, nullptr);

                    if (m_pRenderersData->m_fTearingTest && (m_dpPParam.BackBufferWidth > 22)) {
                        if (m_rcTearing.left + 23 > static_cast<LONG>(m_dpPParam.BackBufferWidth)) {
                            m_rcTearing.left = 0;
                        }
                        m_rcTearing.right = m_rcTearing.left + 4;
                        m_pD3DDev->ColorFill(m_pFIBufferRT1, &m_rcTearing, 0xFFFF0000);
                        m_rcTearing.left += 19;
                        m_rcTearing.right = m_rcTearing.left + 4;
                        m_pD3DDev->ColorFill(m_pFIBufferRT1, &m_rcTearing, 0xFFFF0000);
                        m_rcTearing.left += 7;
                    }
                    hr = m_pD3DDev->StretchRect(m_pFIBufferRT1, nullptr, m_pBackBuffer, nullptr, D3DTEXF_POINT);
                    if (u8InterFrames > 7) {
                        if (bPresentEx) {
                            m_pD3DDev->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
                        } else {
                            m_pSwapChain->Present(nullptr, nullptr, nullptr, nullptr, 0);
                        }
                        m_pD3DDev->SetRenderTarget(3, nullptr);

                        if (m_pRenderersData->m_fTearingTest && (m_dpPParam.BackBufferWidth > 22)) {
                            if (m_rcTearing.left + 23 > static_cast<LONG>(m_dpPParam.BackBufferWidth)) {
                                m_rcTearing.left = 0;
                            }
                            m_rcTearing.right = m_rcTearing.left + 4;
                            m_pD3DDev->ColorFill(m_pFIBufferRT2, &m_rcTearing, 0xFFFF0000);
                            m_rcTearing.left += 19;
                            m_rcTearing.right = m_rcTearing.left + 4;
                            m_pD3DDev->ColorFill(m_pFIBufferRT2, &m_rcTearing, 0xFFFF0000);
                            m_rcTearing.left += 7;
                        }
                        hr = m_pD3DDev->StretchRect(m_pFIBufferRT2, nullptr, m_pBackBuffer, nullptr, D3DTEXF_POINT);
                    }// frame number 4 of pass 2 is always passed to the standard present of the next Paint() call
                }
            }
        }
        // pointer swaps for moving to a new incoming frame
        unsigned __int8 u8FItemp = m_u8FIold;
        m_u8FIold = m_u8FIprevious;
        m_u8FIprevious = m_u8FInext;
        m_u8FInext = u8FItemp;
        // simply swapping m_u8FInext and u8Wsrc won't work, u8Wsrc is a non-static 0 or 1, luckily these textures and surfaces all have the same reference counts
        IDirect3DSurface9* pTmpS = m_apTempWindowSurface[u8FItemp];
        m_apTempWindowSurface[u8FItemp] = m_apTempWindowSurface[u8Wsrc];
        m_apTempWindowSurface[u8Wsrc] = pTmpS;
        IDirect3DTexture9* pTmpT = m_apTempWindowTexture[u8FItemp];
        m_apTempWindowTexture[u8FItemp] = m_apTempWindowTexture[u8Wsrc];
        m_apTempWindowTexture[u8Wsrc] = pTmpT;

        if (m_u8VMR9FrameInterpolationCurrent != 1) {
            // revert linear inter-frame sampling
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(3, D3DSAMP_MAGFILTER, D3DTEXF_POINT)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(3, D3DSAMP_MINFILTER, D3DTEXF_POINT)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(4, D3DSAMP_MAGFILTER, D3DTEXF_POINT)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(4, D3DSAMP_MINFILTER, D3DTEXF_POINT)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(5, D3DSAMP_MAGFILTER, D3DTEXF_POINT)));
            EXECUTE_ASSERT(S_OK == (hr = m_pD3DDev->SetSamplerState(5, D3DSAMP_MINFILTER, D3DTEXF_POINT)));
        }
    } else {
        // final pass
        if (m_dfSurfaceType != D3DFMT_X8R8G8B8) {
            IDirect3DTexture9* pSourceTexture = (bResizerSection || upWindowPassCount) ? m_apTempWindowTexture[u8Wsrc] : (m_pIniatialPixelShader2 || upVideoPassCount) ? m_apTempVideoTexture[u8Vsrc] : m_apVideoTexture[m_u8CurrentMixerSurface];// be careful with this, it's very sensitive to errors
            ASSERT(pSourceTexture);
            hr = m_pD3DDev->SetTexture(0, pSourceTexture);
            ASSERT(m_pBackBuffer);
            hr = m_pD3DDev->SetRenderTarget(0, m_pBackBuffer);
            ASSERT(m_pFinalPixelShader);
            hr = m_pD3DDev->SetPixelShader(m_pFinalPixelShader);

            if (m_u8VMR9DitheringLevelsCurrent == 1 || m_u8VMR9DitheringLevelsCurrent == 2) {
                ASSERT(m_pDitherTexture);
                hr = m_pD3DDev->SetTexture(1, m_pDitherTexture);// set sampler: Dither
            }
            if (m_u8VMR9ColorManagementEnableCurrent == 1) {
                ASSERT(m_pLut3DTexture);
                hr = m_pD3DDev->SetTexture(2, m_pLut3DTexture);// set sampler: Lut3D
            }

            if (m_u8VMR9DitheringLevelsCurrent >= 2) {
                static_assert(RAND_MAX == 0x7FFF, "RAND_MAX changed, update these routines that currently depend on a 15-bit random system");
                // shader constant inputs, only for used for dithering
                __declspec(align(16)) float afConstData[20];
                /* layouts in memory
                dither >= 3:
                rnd, rnd, rnd, rnd

                dither == 2:
                r0r, r0r, r0g, r0g,
                r0b, r0b, ign, ign,
                p0r, p0r, p0r, p0r,
                p0g, p0g, p0g, p0g,
                p0b, p0b, p0b, p0b */

                // group calls together before the main part of the routine
                ptrdiff_t ipRCount = 4 - 1;// default for dither level 3 and over
                if (m_u8VMR9DitheringLevelsCurrent == 2) {
                    reinterpret_cast<__int32*>(afConstData)[8] = rand();
                    ipRCount = 6 - 1;
                }

                do {// fill with temporary random integers
                    reinterpret_cast<__int32*>(afConstData)[ipRCount] = rand() << 8;// shifted left: lined up to fit the 23 mantissa bits of floats
                } while (--ipRCount >= 0);

                // insert at least the 4 random floats required for both dithering types
                // if some of the upper numbers are not yet initialized here, it doesn't matter, this part of the code is practically only slowed down by the first load operation
                // [0, 1) random generator
                static __declspec(align(16)) float const akfOne[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                __m128 xOne = _mm_load_ps(akfOne);
                __m128 xRnd0 = _mm_load_ps(afConstData);
                __m128 xRnd1 = _mm_load_ps(afConstData + 4);
                xRnd0 = _mm_or_ps(xRnd0, xOne);// note that 1.0f has all mantissa bits 0
                xRnd1 = _mm_or_ps(xRnd1, xOne);
                xRnd0 = _mm_sub_ps(xRnd0, xOne);// shift the numbers from interval [1, 2) to [0, 1)
                xRnd1 = _mm_sub_ps(xRnd1, xOne);
                _mm_store_ps(afConstData, xRnd0);
                _mm_store_ps(afConstData + 4, xRnd1);

                UINT uiVector4fCount = 1;// default for dither level 3 and over
                if (m_u8VMR9DitheringLevelsCurrent == 2) {
                    // ±1, 0 projection data for sampling directions generator
                    // 8 configurations, made using 3 random bits:
                    // a = ( 1,  1), b = ( 0,  0)
                    // a = ( 1, -1), b = ( 0,  0)
                    // a = (-1,  1), b = ( 0,  0)
                    // a = (-1, -1), b = ( 0,  0)
                    // a = ( 0,  0), b = ( 1,  1)
                    // a = ( 0,  0), b = ( 1, -1)
                    // a = ( 0,  0), b = (-1,  1)
                    // a = ( 0,  0), b = (-1, -1)
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
                    __m128i xBits4567 = _mm_cvtsi32_si128(reinterpret_cast<__int32*>(afConstData)[8]);// correctly compiles to movd xmmr, m32
                    static __declspec(align(16)) __int32 const aki32SignBits[4] = {0x80000000, 0x80000000, 0x80000000, 0x80000000};
                    __m128i xSignBits = _mm_load_si128(reinterpret_cast<__m128i const*>(aki32SignBits));
                    __m128i xBits0123 = xBits4567;
                    xBits4567 = _mm_slli_epi64(xBits4567, 33);
                    xBits0123 = _mm_or_si128(xBits0123, xBits4567);
                    __m128i xBitsCDE = xBits0123;
                    xBits0123 = _mm_slli_epi64(xBits0123, 2);
                    xBits0123 = _mm_unpacklo_epi64(xBitsCDE, xBits0123);// result: packed vector, the input 32-bit integer is left-shifted by 0, 1, 2 and 3 places
                    // left-shift the numbers to get the random bits to bit 31
                    xBits4567 = xBitsCDE;
                    xBits0123 = xBitsCDE;
                    xBitsCDE = _mm_slli_si128(xBitsCDE, 2);// n, 17, 18, 19
                    xBits4567 = _mm_slli_si128(xBits4567, 3);// 24, 25, 26, 27
                    xBits0123 = _mm_slli_epi64(xBits0123, 28);// 28, 29, 30, 31
                    // create lower or higher bitmasks of the first set
                    xBitsCDE = _mm_srai_epi32(xBitsCDE, 31);
                    // keep only the sign bits of the second set
                    xBits4567 = _mm_and_si128(xBits4567, xSignBits);
                    xBits0123 = _mm_and_si128(xBits0123, xSignBits);
                    // expand the bitmasks of the first set to 64 bits each
                    __m128i xBitsM = _mm_shuffle_epi32(xBitsCDE, _MM_SHUFFLE(3, 2, 1, 1));
                    xBitsCDE = _mm_shuffle_epi32(xBitsCDE, _MM_SHUFFLE(3, 3, 2, 2));
                    // apply the sign bits of the second set on 1.0f
                    xBits4567 = _mm_or_si128(xBits4567, _mm_castps_si128(xOne));
                    xBits0123 = _mm_or_si128(xBits0123, _mm_castps_si128(xOne));
                    // combine the sets to create the finished values
                    __m128i xBitsMn = xBitsM;
                    __m128i xBitsCDEn = xBitsCDE;
                    xBitsM = _mm_and_si128(xBitsM, xBits4567);
                    xBitsCDE = _mm_and_si128(xBitsCDE, xBits0123);
                    xBitsMn = _mm_andnot_si128(xBitsMn, xBits4567);
                    xBitsCDEn = _mm_andnot_si128(xBitsCDEn, xBits0123);
                    // combine upper and lower parts of each set
                    __m128i xBitsCDEt = xBitsCDE;
                    xBitsM = _mm_unpacklo_epi64(xBitsM, xBitsMn);
                    xBitsCDE = _mm_unpacklo_epi64(xBitsCDE, xBitsCDEn);
                    xBitsCDEt = _mm_unpackhi_epi64(xBitsCDEt, xBitsCDEn);
                    // store three sets
                    _mm_store_si128(reinterpret_cast<__m128i*>(afConstData + 8), xBitsM);
                    _mm_store_si128(reinterpret_cast<__m128i*>(afConstData + 12), xBitsCDE);
                    _mm_store_si128(reinterpret_cast<__m128i*>(afConstData + 16), xBitsCDEt);
#else
                    __int32 i32One = 0x3F800000, i32SignBit = 0x80000000;// 0x3F800000 is 1.0f
                    __int32 i32Rn = reinterpret_cast<__int32*>(afConstData)[8];
                    // left-shift the numbers to get the random bits to bit 31 and apply it on one
                    __int32 i32rSignLow1 = (i32Rn << 28 & i32SignBit) | i32One;// bit 3
                    __int32 i32rSignLow2 = (i32Rn << 25 & i32SignBit) | i32One;// bit 6
                    __int32 i32rSignLow0 = i32Rn << 31 | i32One;// bit 0, no masking required
                    __int32 i32rSignHigh0 = (i32Rn << 30 & i32SignBit) | i32One;// bit 1 to bit 31
                    __int32 i32rSignHigh1 = (i32Rn << 27 & i32SignBit) | i32One;// bit 4
                    __int32 i32rSignHigh2 = (i32Rn << 24 & i32SignBit) | i32One;// bit 7
                    // create lower bitmasks, the high bitmasks are just the bitwise negation
                    __int32 i32rLowMask0 = (i32Rn << 29) >> 31;// bit 2
                    __int32 i32rLowMask1 = (i32Rn << 26) >> 31;// bit 5
                    __int32 i32rLowMask2 = (i32Rn << 23) >> 31;// bit 8
                    // apply the masks and store five sets
                    reinterpret_cast<__int32*>(afConstData)[8] = i32rSignLow0 & i32rLowMask0;
                    reinterpret_cast<__int32*>(afConstData)[9] = i32rSignHigh0 & i32rLowMask0;
                    reinterpret_cast<__int32*>(afConstData)[10] = i32rSignLow0 & ~i32rLowMask0;
                    reinterpret_cast<__int32*>(afConstData)[11] = i32rSignHigh0 & ~i32rLowMask0;
                    reinterpret_cast<__int32*>(afConstData)[12] = i32rSignLow1 & i32rLowMask1;
                    reinterpret_cast<__int32*>(afConstData)[13] = i32rSignHigh1 & i32rLowMask1;
                    reinterpret_cast<__int32*>(afConstData)[14] = i32rSignLow1 & ~i32rLowMask1;
                    reinterpret_cast<__int32*>(afConstData)[15] = i32rSignHigh1 & ~i32rLowMask1;
                    reinterpret_cast<__int32*>(afConstData)[16] = i32rSignLow2 & i32rLowMask2;
                    reinterpret_cast<__int32*>(afConstData)[17] = i32rSignHigh2 & i32rLowMask2;
                    reinterpret_cast<__int32*>(afConstData)[18] = i32rSignLow2 & ~i32rLowMask2;
                    reinterpret_cast<__int32*>(afConstData)[19] = i32rSignHigh2 & ~i32rLowMask2;
#endif
                    uiVector4fCount = 5;
                }
                hr = m_pD3DDev->SetPixelShaderConstantF(0, afConstData, uiVector4fCount);
            }
            hr = m_pD3DDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);// draw the rectangle, BaseVertexIndex: 0
        }

        // tear test bars
        if (m_pRenderersData->m_fTearingTest && (m_dpPParam.BackBufferWidth > 22)) {
            if (m_rcTearing.left + 23 > static_cast<LONG>(m_dpPParam.BackBufferWidth)) {
                m_rcTearing.left = 0;
            }
            m_rcTearing.right = m_rcTearing.left + 4;
            m_pD3DDev->ColorFill(m_pBackBuffer, &m_rcTearing, 0xFFFF0000);
            m_rcTearing.left += 19;
            m_rcTearing.right = m_rcTearing.left + 4;
            m_pD3DDev->ColorFill(m_pBackBuffer, &m_rcTearing, 0xFFFF0000);
            m_rcTearing.left += 7;
        }
    }

    m_pD3DDev->EndScene();// always call EndScene() as soon as a rendering loop can't continue and the last operation to the render target was added to the render queue
}

__declspec(nothrow noalias) STDMETHODIMP CDX9AllocatorPresenter::GetAlphaBitmapParameters(__out VMR9AlphaBitmap* pBmpParms)
{
    ASSERT(pBmpParms);

    memcpy(pBmpParms, &m_abVMR9AlphaBitmap, sizeof(VMR9AlphaBitmap));
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP CDX9AllocatorPresenter::SetAlphaBitmap(__in VMR9AlphaBitmap const* pBmpParms)
{
    ASSERT(pBmpParms);

    memcpy(&m_abVMR9AlphaBitmap, pBmpParms, sizeof(VMR9AlphaBitmap));

    // set up the OSD texture
    CAutoLock cRenderLock(&m_csRenderLock);
    if (m_abVMR9AlphaBitmap.dwFlags & VMRBITMAP_DISABLE) {
        if (m_pOSDTexture) {
            m_pOSDTexture->Release();
            m_pOSDTexture = nullptr;
        }
    } else {
        if (!m_pBackBuffer// for during resets, this also disables the function for an uninitialized renderer state
                || (m_dpPParam.BackBufferWidth < 63)) {// the copy constructor can't handle this, as I can't be bothered to add the semantics for a small copy target
            return S_FALSE;
        }

        HBITMAP hBitmap = reinterpret_cast<HBITMAP>(GetCurrentObject(m_abVMR9AlphaBitmap.hdc, OBJ_BITMAP));
        if (!hBitmap) {
            ErrBox(0, L"failed to get a handle to the OSD bitmap\n");
        }

        DIBSECTION info;
        if (!::GetObjectW(hBitmap, sizeof(DIBSECTION), &info)) {
            ErrBox(0, L"failed to get data from the OSD bitmap\n");
        }
        static_assert(sizeof(info.dsBm.bmWidth) == 4, "struct DIBSECTION or platform settings changed");
        if ((static_cast<LONG>(m_dpPParam.BackBufferWidth) != info.dsBm.bmWidth) || (static_cast<LONG>(m_dpPParam.BackBufferHeight) != info.dsBm.bmHeight)) {
            return S_FALSE;// for during resets, don't use the OSD while the previous OSD texture is still around
        }

        HRESULT hr;
        if (m_pOSDTexture) {
            D3DSURFACE_DESC Desc;
            EXECUTE_ASSERT(S_OK == (hr = m_pOSDTexture->GetLevelDesc(0, &Desc)));
            if ((Desc.Width != m_dpPParam.BackBufferWidth) || (Desc.Height != m_dpPParam.BackBufferHeight)) {
                m_pOSDTexture->Release();
                m_pOSDTexture = nullptr;
                goto CreateNewOsdTexture;
            }
        } else {
CreateNewOsdTexture:
            if (FAILED(hr = m_pD3DDev->CreateTexture(m_dpPParam.BackBufferWidth, m_dpPParam.BackBufferHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_pOSDTexture, nullptr))) {
                ErrBox(hr, L"creation of the OSD texture failed\n");
            }
        }

        ASSERT(!m_pUtilTexture);
        if (m_u8OSVersionMajor >= 6) {// Vista and newer only, construct it from a HANDLE
            if (FAILED(hr = m_pD3DDev->CreateTexture(m_dpPParam.BackBufferWidth, m_dpPParam.BackBufferHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &m_pUtilTexture, &info.dsBm.bmBits))) {
                ErrBox(hr, L"creation of the temporary OSD texture failed\n");
            }
        } else {
            if (FAILED(hr = m_pD3DDev->CreateTexture(m_dpPParam.BackBufferWidth, m_dpPParam.BackBufferHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &m_pUtilTexture, nullptr))) {
                ErrBox(hr, L"creation of the temporary OSD texture failed\n");
            }
            D3DLOCKED_RECT lockedRect;
            if (FAILED(hr = m_pUtilTexture->LockRect(0, &lockedRect, nullptr, 0))) {
                ErrBox(hr, L"failed to lock the temporary OSD texture\n");
            }

            unsigned __int32 u32Count = m_dpPParam.BackBufferWidth * m_dpPParam.BackBufferHeight;
            uintptr_t pSrc = reinterpret_cast<uintptr_t>(info.dsBm.bmBits);
            ASSERT(!(pSrc & 15));// if not 16-byte aligned, _mm_load_ps will fail
            uintptr_t pDst = reinterpret_cast<uintptr_t>(lockedRect.pBits);
            ASSERT(!(pDst & 15));// if not 16-byte aligned, _mm_stream_ps will fail

            // the main bulk of the texture is usually far too big to fit into the processor's caches, so non-temporal stores are used here
            if (unsigned __int32 j = u32Count >> 5) do {// excludes the last the last 31 optional values (in the bit shift)
                    __m128 x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                    __m128 x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                    __m128 x2 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 32));
                    __m128 x3 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 48));
                    __m128 x4 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 64));
                    __m128 x5 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 80));
                    __m128 x6 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 96));
                    __m128 x7 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 112));
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

            if (u32Count & 16) {// finalize the last 31 optional values, sorted for aligned access
                __m128 x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                __m128 x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                __m128 x2 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 32));
                __m128 x3 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 48));
                _mm_stream_ps(reinterpret_cast<float*>(pDst), x0);
                _mm_stream_ps(reinterpret_cast<float*>(pDst + 16), x1);
                _mm_stream_ps(reinterpret_cast<float*>(pDst + 32), x2);
                _mm_stream_ps(reinterpret_cast<float*>(pDst + 48), x3);
                pSrc += 64, pDst += 64;
            }
            if (u32Count & 8) {
                __m128 x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                __m128 x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                _mm_stream_ps(reinterpret_cast<float*>(pDst), x0);
                _mm_stream_ps(reinterpret_cast<float*>(pDst + 16), x1);
                pSrc += 32, pDst += 32;
            }
            if (u32Count & 4) {
                __m128 x0 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                _mm_stream_ps(reinterpret_cast<float*>(pDst), x0);
                pSrc += 16, pDst += 16;
            }
            if (u32Count & 2) {
#ifdef _M_X64
                __int64 y = *reinterpret_cast<__int64*>(pSrc);
                _mm_stream_si64x(reinterpret_cast<__int64*>(pDst), y);
#else
                __int32 y = *reinterpret_cast<__int32*>(pSrc);
                __int32 z = *reinterpret_cast<__int32*>(pSrc + 4);
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds
                _mm_stream_si32(reinterpret_cast<__int32*>(pDst), y);
                _mm_stream_si32(reinterpret_cast<__int32*>(pDst + 4), z);
#else
                *reinterpret_cast<__int32*>(pDst) = y;
                *reinterpret_cast<__int32*>(pDst + 4) = z;
#endif
#endif
                pSrc += 8, pDst += 8;
            }
            if (u32Count & 1) {// no address increments for the last possible value
                __int32 y = *reinterpret_cast<__int32*>(pSrc);
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
                _mm_stream_si32(reinterpret_cast<__int32*>(pDst), y);
#else
                *reinterpret_cast<__int32*>(pDst) = y;
#endif
            }

            if (FAILED(hr = m_pUtilTexture->UnlockRect(0))) {
                ErrBox(hr, L"failed to unlock the temporary OSD texture\n");
            }
        }

        if (FAILED(hr = m_pD3DDev->UpdateTexture(m_pUtilTexture, m_pOSDTexture))) {
            ErrBox(hr, L"failed to write the final OSD texture\n");
        }
        m_pUtilTexture->Release();
        m_pUtilTexture = nullptr;
    }
    return S_OK;
}

__declspec(nothrow noalias noinline) void CDX9AllocatorPresenter::ResetMainDevice()
{
    TRACE(L"Video renderer ResetMainDevice()\n");

    if (m_hEvtQuitVSync) {// always close the VSync thread when resetting
        SetEvent(m_hEvtQuitVSync);
        if (m_hVSyncThread) {
            if (WaitForSingleObject(m_hVSyncThread, 10000) == WAIT_TIMEOUT) {
                ASSERT(0);
                TerminateThread(m_hVSyncThread, 0xDEAD);
            }
            CloseHandle(m_hVSyncThread);
            m_hVSyncThread = nullptr;
        }

        CloseHandle(m_hEvtQuitVSync);
        m_hEvtQuitVSync = nullptr;
    }

    if (m_bExFrameSchedulerActive) {
        if (m_dpPParam.Windowed && (m_u16OSVersionMinorMajor == 0x600)) {// disable the DWM qued present mode for Vista
            // m_bExFrameSchedulerActive = false; automatically cleared later on
            DWM_PRESENT_PARAMETERS dpp;
            dpp.cbSize = sizeof(dpp);
            dpp.fQueue = FALSE;
            HRESULT hr = m_fnDwmSetPresentParameters(m_dpPParam.hDeviceWindow, &dpp);
            ASSERT(hr == S_OK);
        } else {// abort the swap chain present queue
            m_pD3DDev->PresentEx(nullptr, nullptr, nullptr, nullptr, D3DPRESENT_FORCEIMMEDIATE | D3DPRESENT_DONOTFLIP);
        }
    }

    // these always need to be reset, but these may already be reset at this point because the reset sequence in Paint() clears them
    if (m_pBackBuffer) {
        m_pBackBuffer->Release();
        m_pBackBuffer = nullptr;
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }

    if (!m_bPartialExDeviceReset) {// note: partial resets are available for Vista and newer only
        // optional parts
        if (m_pLine) {
            m_pLine->Release();
            m_pLine = nullptr;
        }
        if (m_pFont) {
            m_pFont->Release();
            m_pFont = nullptr;
        }
        if (m_pStatsRectVBuffer) {
            m_pStatsRectVBuffer->Release();
            m_pStatsRectVBuffer = nullptr;
        }
        if (m_pSubBlendVBuffer) {
            m_pSubBlendVBuffer->Release();
            m_pSubBlendVBuffer = nullptr;
        }
        if (m_pVBuffer) {
            m_pVBuffer->Release();
            m_pVBuffer = nullptr;
            if (m_pResizerPixelShaderX) {
                m_pResizerPixelShaderX->Release();
                m_pResizerPixelShaderX = nullptr;
                if (m_pPreResizerHorizontalPixelShader) {
                    m_pPreResizerHorizontalPixelShader->Release();
                    m_pPreResizerHorizontalPixelShader = nullptr;
                }
                if (m_pPreResizerVerticalPixelShader) {
                    m_pPreResizerVerticalPixelShader->Release();
                    m_pPreResizerVerticalPixelShader = nullptr;
                }
                if (m_pResizerPixelShaderY) {
                    m_pResizerPixelShaderY->Release();
                    m_pResizerPixelShaderY = nullptr;
                    m_pIntermediateResizeSurface->Release();
                    m_pIntermediateResizeSurface = nullptr;
                    m_pIntermediateResizeTexture->Release();
                    m_pIntermediateResizeTexture = nullptr;
                }
            }
        }
        ptrdiff_t i = MAX_VIDEO_SURFACES - 1;
        do {
            if (m_apVideoSurface[i]) {
                m_apVideoSurface[i]->Release();
                m_apVideoSurface[i] = nullptr;
                m_apVideoTexture[i]->Release();
                m_apVideoTexture[i] = nullptr;
            }
        } while (--i >= 0);
        if (m_apTempVideoSurface[0]) {
            m_apTempVideoSurface[0]->Release();
            m_apTempVideoSurface[0] = nullptr;
            m_apTempVideoTexture[0]->Release();
            m_apTempVideoTexture[0] = nullptr;
        }
        if (m_apTempVideoSurface[1]) {
            m_apTempVideoSurface[1]->Release();
            m_apTempVideoSurface[1] = nullptr;
            m_apTempVideoTexture[1]->Release();
            m_apTempVideoTexture[1] = nullptr;
        }
        if (m_apTempWindowSurface[0]) {
            m_apTempWindowSurface[0]->Release();
            m_apTempWindowSurface[0] = nullptr;
            m_apTempWindowTexture[0]->Release();
            m_apTempWindowTexture[0] = nullptr;
        }
        if (m_apTempWindowSurface[1]) {
            m_apTempWindowSurface[1]->Release();
            m_apTempWindowSurface[1] = nullptr;
            m_apTempWindowTexture[1]->Release();
            m_apTempWindowTexture[1] = nullptr;
        }
        if (m_apTempWindowSurface[2]) {
            m_apTempWindowSurface[2]->Release();
            m_apTempWindowSurface[2] = nullptr;
            m_apTempWindowSurface[3]->Release();
            m_apTempWindowSurface[3] = nullptr;
            m_apTempWindowSurface[4]->Release();
            m_apTempWindowSurface[4] = nullptr;
            m_apTempWindowTexture[2]->Release();
            m_apTempWindowTexture[2] = nullptr;
            m_apTempWindowTexture[3]->Release();
            m_apTempWindowTexture[3] = nullptr;
            m_apTempWindowTexture[4]->Release();
            m_apTempWindowTexture[4] = nullptr;
            if (m_apFIPreSurface[0]) {
                m_apFIPreSurface[0]->Release();
                m_apFIPreSurface[0] = nullptr;
                m_apFIPreSurface[1]->Release();
                m_apFIPreSurface[1] = nullptr;
                m_apFIPreSurface[2]->Release();
                m_apFIPreSurface[2] = nullptr;
                m_apFIPreSurface[3]->Release();
                m_apFIPreSurface[3] = nullptr;
                m_apFIPreTexture[0]->Release();
                m_apFIPreTexture[0] = nullptr;
                m_apFIPreTexture[1]->Release();
                m_apFIPreTexture[1] = nullptr;
                m_apFIPreTexture[2]->Release();
                m_apFIPreTexture[2] = nullptr;
                m_apFIPreTexture[3]->Release();
                m_apFIPreTexture[3] = nullptr;
            }
        }
        if (m_apFIPixelShader[0]) {
            m_apFIPixelShader[0]->Release();
            m_apFIPixelShader[0] = nullptr;
            m_apFIPixelShader[1]->Release();
            m_apFIPixelShader[1] = nullptr;
            m_apFIPixelShader[2]->Release();
            m_apFIPixelShader[2] = nullptr;
            m_apFIPixelShader[3]->Release();
            m_apFIPixelShader[3] = nullptr;
            if (m_apFIPrePixelShader[0]) {
                m_apFIPrePixelShader[0]->Release();
                m_apFIPrePixelShader[0] = nullptr;
                m_apFIPrePixelShader[1]->Release();
                m_apFIPrePixelShader[1] = nullptr;
                m_apFIPrePixelShader[2]->Release();
                m_apFIPrePixelShader[2] = nullptr;
            }
        }
        if (m_pFIBufferRT0) {
            m_pFIBufferRT0->Release();
            m_pFIBufferRT0 = nullptr;
            if (m_pFIBufferRT1) {
                m_pFIBufferRT1->Release();
                m_pFIBufferRT1 = nullptr;
                if (m_pFIBufferRT2) {
                    m_pFIBufferRT2->Release();
                    m_pFIBufferRT2 = nullptr;
                }
            }
        }
        if (m_pFinalPixelShader) {
            m_pFinalPixelShader->Release();
            m_pFinalPixelShader = nullptr;
        }
        if (m_pSubtitlePassPixelShader) {
            m_pSubtitlePassPixelShader->Release();
            m_pSubtitlePassPixelShader = nullptr;
            ASSERT(m_pOSDPassPixelShader);// created as a pair
            m_pOSDPassPixelShader->Release();
            m_pOSDPassPixelShader = nullptr;
        }
        if (m_pIniatialPixelShader2) {
            m_pIniatialPixelShader2->Release();
            m_pIniatialPixelShader2 = nullptr;
            if (m_pIniatialPixelShader0) {
                m_pIniatialPixelShader0->Release();
                m_pIniatialPixelShader0 = nullptr;
                if (m_pIniatialPixelShader1) {
                    m_pIniatialPixelShader1->Release();
                    m_pIniatialPixelShader1 = nullptr;
                }
            }
        }
        if (m_pDitherTexture) {
            m_pDitherTexture->Release();
            m_pDitherTexture = nullptr;
        }
        if (m_pLut3DTexture) {
            m_pLut3DTexture->Release();
            m_pLut3DTexture = nullptr;
        }
        if (m_pOSDTexture) {
            m_pOSDTexture->Release();
            m_pOSDTexture = nullptr;
        }
        if (m_pSubtitleTexture) {
            m_pSubtitleTexture->Release();
            m_pSubtitleTexture = nullptr;
        }
        // pointers guaranteed to be valid, these resources are re-created in CreateDevice()
        m_pSubPicQueue->Release();
        m_pSubPicQueue = nullptr;
        m_pSubPicAllocator->Release();
        m_pSubPicAllocator = nullptr;
        m_pIndexBuffer->Release();
        m_pIndexBuffer = nullptr;

        // external pixel shaders; release the interface pointers, keep the base data and strings
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

        // todo: set DMode/DModeEx and the m_dpPParam refresh rate items to [UINT display refresh rate] if the mode should be changed on exit, Width and Height can be modified, too, this does need a reorganization of the m_uiBaseRefreshRate item
        if (m_dpPParam.BackBufferFormat == D3DFMT_A2R10G10B10) {// 10-bit mode needs a proper shutdown
            ASSERT(m_u8OSVersionMajor >= 6);// Vista and newer only
            m_dpPParam.BackBufferCount = 1;
            m_dpPParam.BackBufferFormat = D3DFMT_X8R8G8B8;
            m_dpPParam.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
            D3DDISPLAYMODEEX DModeEx = {sizeof(D3DDISPLAYMODEEX), m_dpPParam.BackBufferWidth, m_dpPParam.BackBufferHeight, m_uiBaseRefreshRate, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_PROGRESSIVE};
            m_pD3DDev->ResetEx(&m_dpPParam, &DModeEx);
        }
        if (!m_dpPParam.Windowed) {// restore to windowed mode
            m_dpPParam.BackBufferWidth = 1;
            m_dpPParam.BackBufferHeight = 1;
            m_dpPParam.BackBufferCount = 1;
            m_dpPParam.Windowed = TRUE;
            m_dpPParam.FullScreen_RefreshRateInHz = 0;
            m_dpPParam.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
            m_pD3DDev->Reset(&m_dpPParam);
        }

        m_pD3DDev->Release();
        m_pD3DDev = nullptr;
        // m_pD3D is kept, it doesn't need to be renewed on reset

        // if present, delete the extra window
        if (m_hUtilityWnd) {// the window class only needs to be cleared at shutdown
            EXECUTE_ASSERT(DestroyWindow(m_hUtilityWnd));
            m_hUtilityWnd = nullptr;
        }
    } else if (m_pSubPicProvider) {// for partial resets, keep the queue tread from handling subtitle textures during the reset, it is re-instated inside CreateDevice()
        m_pSubPicQueue->SetSubPicProvider(nullptr);
    }

    CStringW strError;
    CreateDevice(&strError);
    if (!strError.IsEmpty()) {
        // ErrBox() copy, error message box routine
        // the neutral "error" title caption will be used
        MessageBoxW(nullptr, strError, nullptr, MB_SYSTEMMODAL | MB_ICONERROR);
        strError.Empty();
        abort();
    }
}

__declspec(nothrow noalias) HRESULT CDX9AllocatorPresenter::GetDIB(__out_opt void* pDib, __inout size_t* pSize)
{
    ASSERT(pSize);

    if ((m_u32VideoWidth < 63) || (m_u32VideoHeight < 63)) {// the copy constructor can't handle this, as I can't be bothered to add the semantics for a small copy target, this also disables the function for an uninitialized renderer state
        return E_ABORT;
    }
    unsigned __int32 u32SizeImage = m_u32VideoHeight * m_u32VideoWidth << 2;
    unsigned __int32 u32Required = sizeof(BITMAPINFOHEADER) + u32SizeImage;
    if (!pDib) {
        *pSize = u32Required;
        return S_OK;
    }
    if (*pSize < u32Required) {
        return E_OUTOFMEMORY;
    }

    CAutoLock cRenderLock(&m_csRenderLock);
    HRESULT hr;
    IDirect3DSurface9* pSurface;
    if (FAILED(hr = m_pD3DDev->CreateOffscreenPlainSurface(m_u32VideoWidth, m_u32VideoHeight, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pSurface, nullptr))) {
        return hr;
    }

    if (FAILED(hr = m_fnD3DXLoadSurfaceFromSurface(pSurface, nullptr, nullptr, m_apVideoSurface[m_u8CurrentMixerSurface], nullptr, nullptr, (m_dfSurfaceType == D3DFMT_X8R8G8B8) ? D3DX_FILTER_NONE : D3DX_FILTER_POINT | D3DX_FILTER_DITHER, 0))) {
        goto exit;
    }

    D3DLOCKED_RECT r;
    if (FAILED(hr = pSurface->LockRect(&r, nullptr, D3DLOCK_READONLY))) {
        goto exit;
    }

    BITMAPINFOHEADER* bih = reinterpret_cast<BITMAPINFOHEADER*>(pDib);
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biWidth = m_u32VideoWidth;
    bih->biHeight = m_u32VideoHeight;
    bih->biPlanes = 1;
    bih->biBitCount = 32;
    bih->biCompression = 0;
    bih->biSizeImage = u32SizeImage;
    bih->biXPelsPerMeter = 0;
    bih->biYPelsPerMeter = 0;
    bih->biClrUsed = 0;
    bih->biClrImportant = 0;

    // the DIB receiver requires the picture to be upside-down...
    size_t upWidth = m_u32VideoWidth, upHeight = m_u32VideoHeight, upPitch = upWidth << 2, upPitchOffset = r.Pitch - upPitch;
    uintptr_t pSrc = reinterpret_cast<uintptr_t>(r.pBits);// start of the first source line
    uintptr_t pRow = reinterpret_cast<uintptr_t>(bih + 1) + (upHeight - 1) * upPitch;// start of the last line, with padding for the header
    // note: pRow and pDst only keep 4-byte alignment in the realigning loop, for the aligned loop, allocation of the storage makes it video bus cache line aligned

    size_t upCount = upWidth;
    uintptr_t pDst = pRow;
    if (CPUGetEnabledExtensions() & CPUF_SUPPORTS_SSE41) {// SSE4.1 code path
        __m128i x0, x1, x2, x3, x4, x5, x6, x7;
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
                ASSERT(!(pSrc & 12));// pitch alignment in GPU memory is always at least 16 bytes, but most will align to 32 bytes
                if (pSrc & 16) {// 16-to-32-byte alignment
                    x1 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc));
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst), x1);
                    pSrc += 16;
                    pDst += 16;
                    upCount -= 4;
                }
                if (pSrc & 32) {// 32-to-64-byte alignment
                    x2 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc));
                    x3 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16));
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst), x2);
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 16), x3);
                    pSrc += 32;
                    pDst += 32;
                    upCount -= 8;
                }
                if (pSrc & 64) {// 64-to-128-byte alignment
SkipFirst64baSSE41rl:
                    x4 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc));
                    x5 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16));
                    x6 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32));
                    x7 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48));
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst), x4);
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 16), x5);
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 32), x6);
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 48), x7);
                    pSrc += 64;
                    pDst += 64;
                    upCount -= 16;
                }
SkipFirst128baSSE41rl:
                ASSERT(!(pSrc & 127));// if not 128-byte aligned, the loop is implemented wrong

                size_t j = upCount >> 5;
                do {
                    x0 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc));
                    x1 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16));
                    x2 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32));
                    x3 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48));
                    x4 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 64));
                    x5 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 80));
                    x6 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 96));
                    x7 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 112));
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst), x0);
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 16), x1);
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 32), x2);
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 48), x3);
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 64), x4);
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 80), x5);
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 96), x6);
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 112), x7);
                    pSrc += 128;
                    pDst += 128;
                } while (--j);

                // finalize the last 31 optional values, sorted for aligned access
                if (upCount & 16) {
                    x0 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc));
                    x1 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16));
                    x2 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32));
                    x3 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48));
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst), x0);
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 16), x1);
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 32), x2);
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 48), x3);
                    pSrc += 64;
                    pDst += 64;
                }
                if (upCount & 15) {
                    x4 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc));
                    x5 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16));
                    x6 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32));
                    x7 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48));
                    if (upCount & 8) {
                        _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst), x4);
                        _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 16), x5);
                        pSrc += 32;
                        pDst += 32;
                    }
                    if (upCount & 4) {
                        _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst), x6);
                        pSrc += 16;
                        pDst += 16;
                    }
                    if (upCount & 2) {
                        _mm_storel_epi64(reinterpret_cast<__m128i*>(pDst), x7);
                        x7 = _mm_unpackhi_epi64(x7, x7);// move high part to low
                        pSrc += 8;
                        pDst += 8;
                    }
                    if (upCount & 1) {
                        *reinterpret_cast<__int32*>(pDst) = _mm_cvtsi128_si32(x7);// correctly compiles to movd m32, xmmr
                        pSrc += 4;// no pDst address increment for the last possible value
                    }
                }

                pRow -= upPitch;
            } while (--upHeight);
        } else {// aligned loop
            ASSERT(!upPitchOffset);// high alignment and locking an entire texture means pitch == width*unitsize
            upHeight -= 62;
            if (pSrc & 64) {// pre-load if the source needs 64-to-128-byte re-alignment (a 1-in-2 chance on 64-byte cache line systems)
                x4 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc));
                x5 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16));
                x6 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32));
                x7 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48));
                goto SkipFirst64baSSE41al;
            }
            goto SkipFirst128baSSE41al;

            do {// streaming the larger bottom part
                upCount = upWidth;
                pDst = pRow;

                if (pSrc & 64) {// 64-to-128-byte alignment
SkipFirst64baSSE41al:
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst + 64), x4);
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst + 80), x5);
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst + 96), x6);
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst + 112), x7);
                    pSrc += 64;
                    pDst += 64;
                    upCount -= 16;
                }
SkipFirst128baSSE41al:
                ASSERT(!(pSrc & 127));// if not 128-byte aligned, the loop is implemented wrong

                size_t j = upCount >> 5;
                do {
                    x0 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc));
                    x1 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16));
                    x2 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32));
                    x3 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48));
                    x4 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 64));
                    x5 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 80));
                    x6 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 96));
                    x7 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 112));
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst), x0);
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst + 16), x1);
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst + 32), x2);
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst + 48), x3);
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst + 64), x4);
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst + 80), x5);
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst + 96), x6);
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst + 112), x7);
                    pSrc += 128;
                    pDst += 128;
                } while (--j);

                if (upCount & 16) {// finalize the last 16 optional values
                    x0 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc));
                    x1 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16));
                    x2 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32));
                    x3 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48));
                    x4 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 64));
                    x5 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 80));
                    x6 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 96));
                    x7 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 112));
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst), x0);
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst + 16), x1);
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst + 32), x2);
                    _mm_stream_si128(reinterpret_cast<__m128i*>(pDst + 48), x3);
                    pSrc += 64;// no pDst address increment for the last possible value
                }

                pRow -= upPitch;
            } while (--upHeight);

            upHeight = 62;
            do {// storing the top part, this will usually be the first set of data to be re-requested
                upCount = upWidth;
                pDst = pRow;

                if (pSrc & 64) {// 64-to-128-byte alignment
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst + 64), x4);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst + 80), x5);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst + 96), x6);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst + 112), x7);
                    pSrc += 64;
                    pDst += 64;
                    upCount -= 16;
                }

                size_t j = upCount >> 5;
                do {
                    x0 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc));
                    x1 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16));
                    x2 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32));
                    x3 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48));
                    x4 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 64));
                    x5 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 80));
                    x6 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 96));
                    x7 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 112));
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst), x0);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst + 16), x1);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst + 32), x2);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst + 48), x3);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst + 64), x4);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst + 80), x5);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst + 96), x6);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst + 112), x7);
                    pSrc += 128;
                    pDst += 128;
                } while (--j);

                if (upCount & 16) {// finalize the last 16 optional values
                    x0 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc));
                    x1 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 16));
                    x2 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 32));
                    x3 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 48));
                    x4 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 64));
                    x5 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 80));
                    x6 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 96));
                    x7 = _mm_stream_load_si128(reinterpret_cast<__m128i*>(pSrc + 112));
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst), x0);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst + 16), x1);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst + 32), x2);
                    _mm_store_si128(reinterpret_cast<__m128i*>(pDst + 48), x3);
                    pSrc += 64;// no pDst address increment for the last possible value
                }

                pRow -= upPitch;
            } while (--upHeight);
        }
    } else {// SSE code path
        __m128 x0, x1, x2, x3, x4, x5, x6, x7;
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
                ASSERT(!(pSrc & 12));// pitch alignment in GPU memory is always at least 16 bytes, but most will align to 32 bytes
                if (pSrc & 16) {// 16-to-32-byte alignment
                    x1 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst), x1);
                    pSrc += 16;
                    pDst += 16;
                    upCount -= 4;
                }
                if (pSrc & 32) {// 32-to-64-byte alignment
                    x2 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                    x3 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst), x2);
                    _mm_storeu_ps(reinterpret_cast<float*>(pDst + 16), x3);
                    pSrc += 32;
                    pDst += 32;
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
                    pSrc += 64;
                    pDst += 64;
                    upCount -= 16;
                }
SkipFirst128baSSErl:
                ASSERT(!(pSrc & 127));// if not 128-byte aligned, the loop is implemented wrong

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
                    pSrc += 128;
                    pDst += 128;
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
                    pSrc += 64;
                    pDst += 64;
                }
                if (upCount & 15) {
                    x4 = _mm_load_ps(reinterpret_cast<float*>(pSrc));
                    x5 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 16));
                    x6 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 32));
                    x7 = _mm_load_ps(reinterpret_cast<float*>(pSrc + 48));
                    if (upCount & 8) {
                        _mm_storeu_ps(reinterpret_cast<float*>(pDst), x4);
                        _mm_storeu_ps(reinterpret_cast<float*>(pDst + 16), x5);
                        pSrc += 32;
                        pDst += 32;
                    }
                    if (upCount & 4) {
                        _mm_storeu_ps(reinterpret_cast<float*>(pDst), x6);
                        pSrc += 16;
                        pDst += 16;
                    }
                    if (upCount & 2) {
                        _mm_storel_pi(reinterpret_cast<__m64*>(pDst), x7);// not an MMX function
                        x7 = _mm_movehl_ps(x7, x7);// move high part to low
                        pSrc += 8;
                        pDst += 8;
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
                    pSrc += 64;
                    pDst += 64;
                    upCount -= 16;
                }
SkipFirst128baSSEal:
                ASSERT(!(pSrc & 127));// if not 128-byte aligned, the loop is implemented wrong

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
                    pSrc += 128;
                    pDst += 128;
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
                    _mm_stream_ps(reinterpret_cast<float*>(pDst), x0);
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 16), x1);
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 32), x2);
                    _mm_stream_ps(reinterpret_cast<float*>(pDst + 48), x3);
                    pSrc += 64;// no pDst address increment for the last possible value
                }

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
                    pSrc += 64;
                    pDst += 64;
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
                    pSrc += 128;
                    pDst += 128;
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

                    _mm_store_ps(reinterpret_cast<float*>(pDst), x0);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 16), x1);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 32), x2);
                    _mm_store_ps(reinterpret_cast<float*>(pDst + 48), x3);
                    pSrc += 64;// no pDst address increment for the last possible value
                }

                pRow -= upPitch;
            } while (--upHeight);
        }
    }

    hr = pSurface->UnlockRect();
exit:
    pSurface->Release();
    return hr;
}

struct CUSTOMSHADERTRANSFORM {uintptr_t* pupRetError; CDX9AllocatorPresenter::D3DCompilePtr fnD3DCompile; D3D_SHADER_MACRO const* macros; LPCSTR pProfile; Shader const* pInpShader; IDirect3DDevice9* pD3DDev; CDX9AllocatorPresenter::EXTERNALSHADER* pceShader;};

static __declspec(nothrow noalias noinline) DWORD WINAPI CustShadCThreadStatic(__in LPVOID lpParam)
{
    ASSERT(lpParam);

    CUSTOMSHADERTRANSFORM* const ptf = reinterpret_cast<CUSTOMSHADERTRANSFORM*>(lpParam);
    ASSERT(ptf->pupRetError);
    ASSERT(ptf->fnD3DCompile);
    ASSERT(ptf->macros);
    ASSERT(ptf->pProfile);
    ASSERT(ptf->pInpShader);
    ASSERT(ptf->pD3DDev);
    ASSERT(ptf->pceShader);
    ASSERT(!ptf->pceShader->pPixelShader);
    ASSERT(!ptf->pceShader->pSrcData);
    ASSERT(!ptf->pceShader->strSrcData);
    ASSERT(!ptf->pceShader->u32SrcLen);

    CString const& strIn = ptf->pInpShader->srcdata;
    UINT uiStrL = strIn.GetLength();
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
    // allocate char string
    UINT i = uiStrL;
    char* pStr;
    __m128i const* pSrc = reinterpret_cast<__m128i const*>(static_cast<wchar_t const*>(strIn));
    __m128i* pDst;
    __m128i x0, x1, x2, x3, x4, x5, x6, x7;
    ASSERT(!(reinterpret_cast<uintptr_t>(pSrc) & 7));// it's assured that the input string is 8-byte aligned
    UINT uiRoundedAllocSize = uiStrL + 15 & ~15;
    if (reinterpret_cast<uintptr_t>(pSrc) & 8) {
        // take care of misalignment
        pStr = reinterpret_cast<char*>(_aligned_offset_malloc(uiRoundedAllocSize + 4, 16, 4));// we don't have to append a trailing 0 char, the size is rounded up to modulo 16 bytes, an offset is applied to take care of the 4 first characters for re-alignment
        if (!pStr) {
            ASSERT(0);
            *ptf->pupRetError = 1;// out of memory error
            return 0;
        }
        // store 4 characters
        x7 = _mm_loadl_epi64(pSrc);
        x7 = _mm_packus_epi16(x7, x7);
        *reinterpret_cast<__int32*>(pStr) = _mm_cvtsi128_si32(x7);// correctly compiles to movd m32, xmmr
        pSrc = reinterpret_cast<__m128i const*>(reinterpret_cast<wchar_t const*>(pSrc) + 4);
        pDst = reinterpret_cast<__m128i*>(pStr + 4);
        i -= 4;
    } else {
        pStr = reinterpret_cast<char*>(_aligned_malloc(uiRoundedAllocSize, 16));// we don't have to append a trailing 0 char, the size is rounded up to modulo 16 bytes
        if (!pStr) {
            ASSERT(0);
            *ptf->pupRetError = 1;// out of memory error
            return 0;
        }
        pDst = reinterpret_cast<__m128i*>(pStr);
    }
    ASSERT(!(reinterpret_cast<uintptr_t>(pSrc) & 15));
    ASSERT(!(reinterpret_cast<uintptr_t>(pDst) & 15));
    ptf->pceShader->strSrcData = pStr;

    // copy while quickly converting wchar_t to char, we don't need to preserve characters beyond 127
    if (i >>= 6) do {
            x0 = _mm_load_si128(pSrc);
            x1 = _mm_load_si128(pSrc + 1);
            x2 = _mm_load_si128(pSrc + 2);
            x3 = _mm_load_si128(pSrc + 3);
            x4 = _mm_load_si128(pSrc + 4);
            x5 = _mm_load_si128(pSrc + 5);
            x6 = _mm_load_si128(pSrc + 6);
            x7 = _mm_load_si128(pSrc + 7);
            x0 = _mm_packus_epi16(x0, x1);
            x2 = _mm_packus_epi16(x2, x3);
            x4 = _mm_packus_epi16(x4, x5);
            x6 = _mm_packus_epi16(x6, x7);
            _mm_store_si128(pDst, x0);
            _mm_store_si128(pDst + 1, x2);
            _mm_store_si128(pDst + 2, x4);
            _mm_store_si128(pDst + 3, x6);
            pSrc += 8;
            pDst += 4;
        } while (--i);

    if (uiStrL & 32) {
        x0 = _mm_load_si128(pSrc);
        x1 = _mm_load_si128(pSrc + 1);
        x2 = _mm_load_si128(pSrc + 2);
        x3 = _mm_load_si128(pSrc + 3);
        x0 = _mm_packus_epi16(x0, x1);
        x2 = _mm_packus_epi16(x2, x3);
        _mm_store_si128(pDst, x0);
        _mm_store_si128(pDst + 1, x2);
        pSrc += 4;
        pDst += 2;
    }
    if (uiStrL & 16) {
        x4 = _mm_load_si128(pSrc);
        x5 = _mm_load_si128(pSrc + 1);
        x4 = _mm_packus_epi16(x4, x5);
        _mm_store_si128(pDst, x4);
        pSrc += 2;
        pDst += 1;
    }
    if (uiStrL & 15) {
        x6 = _mm_load_si128(pSrc);
        x7 = x6;
        if ((uiStrL & 8) && (uiStrL & 7)) {
            if ((uiStrL & 4) && (uiStrL & 3)) {
                x7 = _mm_load_si128(pSrc + 1);
            } else {
                x7 = _mm_loadl_epi64(pSrc + 1);
            }
        }
        x6 = _mm_packus_epi16(x6, x7);
        _mm_store_si128(pDst, x6);
    }
#else
    // allocate char string
    char* pStr = reinterpret_cast<char*>(malloc(uiStrL));// we don't have to append a trailing 0 char
    if (!pStr) {
        ASSERT(0);
        *ptf->pupRetError = 1;// out of memory error
        return 0;
    }

    // copy while quickly converting wchar_t to char, we don't need to preserve characters beyond 127
    UINT i = uiStrL;
    char* pDst = pStr;
    char const* pSrc = reinterpret_cast<char const*>(static_cast<wchar_t const*>(strIn));
    do {
        *pDst++ = pSrc[0] | pSrc[1];// higher Unicode pages may have valid characters with the bottom 8 bits 0, to not possibly embed a 0 here, OR the lower and higher bytes
        pSrc += 2;
    } while (--i);
#endif

    // compile
    CDX9AllocatorPresenter::EXTERNALSHADER& ceShader = *ptf->pceShader;
    HRESULT hr;
    if (FAILED(hr = ptf->fnD3DCompile(pStr, uiStrL, nullptr, ptf->macros, nullptr, "main", ptf->pProfile, D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION, 0, &ceShader.pSrcData, nullptr))) {
        ASSERT(0);
        goto ErrorReportLabel;
    }
    if (FAILED(hr = ptf->pD3DDev->CreatePixelShader(reinterpret_cast<DWORD*>(ceShader.pSrcData->GetBufferPointer()), &ceShader.pPixelShader))) {
        ASSERT(0);
        goto ErrorReportLabel;
    }
    return 0;
ErrorReportLabel:
    *ptf->pupRetError = reinterpret_cast<uintptr_t>(&ptf->pInpShader->label);// shader error, report the label
    return 0;
}

__declspec(nothrow noalias) uintptr_t CDX9AllocatorPresenter::SetPixelShaders(__in_ecount(2) CAtlList<Shader const*> const aList[2])
{
    CAutoLock cRenderLock(&m_csRenderLock);
    // clear stages first
    POSITION pos = m_apCustomPixelShaders[0].GetHeadPosition();
    if (pos) {
        do {
            EXTERNALSHADER& ceShader = m_apCustomPixelShaders[0].GetNext(pos);
            if (ceShader.pSrcData) {
                ULONG u = ceShader.pSrcData->Release();
                ASSERT(!u);
                if (ceShader.pPixelShader) {
                    u = ceShader.pPixelShader->Release();
                    ASSERT(!u);
                }
            } else {
                ASSERT(!ceShader.pPixelShader);
            }
            ASSERT(ceShader.strSrcData);
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
            _aligned_free(ceShader.strSrcData);
#else
            free(ceShader.strSrcData);
#endif
        } while (pos);
        m_apCustomPixelShaders[0].RemoveAll();
    }
    pos = m_apCustomPixelShaders[1].GetHeadPosition();
    if (pos) {
        do {
            EXTERNALSHADER& ceShader = m_apCustomPixelShaders[1].GetNext(pos);
            if (ceShader.pSrcData) {
                ULONG u = ceShader.pSrcData->Release();
                ASSERT(!u);
                if (ceShader.pPixelShader) {
                    u = ceShader.pPixelShader->Release();
                    ASSERT(!u);
                }
            } else {
                ASSERT(!ceShader.pPixelShader);
            }
            ASSERT(ceShader.strSrcData);
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
            _aligned_free(ceShader.strSrcData);
#else
            free(ceShader.strSrcData);
#endif
        } while (pos);
        m_apCustomPixelShaders[1].RemoveAll();
    }

    size_t upCount0 = aList[0].GetCount(), upCount1 = aList[1].GetCount(), upTotalCount = upCount0 + upCount1;
    if (!upTotalCount) {
        return 0;
    }

    uintptr_t upRetError = 0;// default return status success
    CUSTOMSHADERTRANSFORM* pTransforms = reinterpret_cast<CUSTOMSHADERTRANSFORM*>(malloc(sizeof(CUSTOMSHADERTRANSFORM) * upTotalCount + sizeof(HANDLE) * (upTotalCount - 1)));// note: transform 0 is handled in this thread
    if (!pTransforms) {
        ASSERT(0);
        return 1;// out of memory error
    }

    // partial initialization
    CUSTOMSHADERTRANSFORM* pIter = pTransforms;
    size_t i = upTotalCount;
    do {
        pIter->pupRetError = &upRetError;
        pIter->fnD3DCompile = m_fnD3DCompile;
        pIter->macros = m_aShaderMacros;
        pIter->pProfile = m_pProfile;
        pIter->pD3DDev = m_pD3DDev;
        // pInpShader and pceShader get initialized later on
        ++pIter;
    } while (--i);
    ASSERT(pIter == pTransforms + upTotalCount);// end of the array
    pIter = pTransforms;// the next loops interate over the array as well

    if (upCount0) {
        pos = aList[0].GetHeadPosition();
        do {
            POSITION outpos = m_apCustomPixelShaders[0].AddTail();// add an empty item
            if (!pos) {
                ASSERT(0);
                free(pTransforms);
                ClearPixelShaders(1);
                return 1;// out of memory error
            }
            EXTERNALSHADER& ceShader = m_apCustomPixelShaders[0].GetAt(outpos);
            ceShader.pPixelShader = nullptr;
            ceShader.pSrcData = nullptr;
            ceShader.strSrcData = nullptr;
            ceShader.u32SrcLen = 0;
            pIter->pceShader = &ceShader;
            pIter->pInpShader = aList[0].GetNext(pos);
            ++pIter;
        } while (pos);
    }

    if (upCount1) {
        pos = aList[1].GetHeadPosition();
        do {
            POSITION outpos = m_apCustomPixelShaders[1].AddTail();// add an empty item
            if (!pos) {
                ASSERT(0);
                free(pTransforms);
                ClearPixelShaders(3);
                return 1;// out of memory error
            }
            EXTERNALSHADER& ceShader = m_apCustomPixelShaders[1].GetAt(outpos);
            ceShader.pPixelShader = nullptr;
            ceShader.pSrcData = nullptr;
            ceShader.strSrcData = nullptr;
            ceShader.u32SrcLen = 0;
            pIter->pceShader = &ceShader;
            pIter->pInpShader = aList[1].GetNext(pos);
            ++pIter;
        } while (pos);
    }
    ASSERT(pIter == pTransforms + upTotalCount);// end of the array

    if (upTotalCount > 1) {
        HANDLE* hProcesses = reinterpret_cast<HANDLE*>(pIter);
        pIter = pTransforms;
        // note: transform 0 is handled in this thread
        ptrdiff_t j = upTotalCount - 2;
        do {
            ++pIter;
            hProcesses[j] = ::CreateThread(nullptr, 0x20000, CustShadCThreadStatic, pIter, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
            if (!hProcesses[j]) {
                goto CustShadCreateThreadFailed;
            }
        } while (--j >= 0);
        goto CustShadCreateThreadSucceeded;

CustShadCreateThreadFailed:
        i = 0;
        do {
            if (!hProcesses[i]) {
                break;
            }
            TerminateThread(hProcesses[i], 0xDEAD);
            CloseHandle(hProcesses[i]);
            ++i;
        } while (i < upTotalCount - 1);
        free(pTransforms);
        ClearPixelShaders(3);
        return 1;// out of memory error
CustShadCreateThreadSucceeded:

        CustShadCThreadStatic(pTransforms);

        ASSERT(upTotalCount - 1 <= MAXDWORD);
        if (WaitForMultipleObjects(static_cast<DWORD>(upTotalCount - 1), hProcesses, TRUE, 60000) == WAIT_TIMEOUT) {// 1 minute should be much more than enough
            ASSERT(0);
            j = upTotalCount - 2;
            do {
                TerminateThread(hProcesses[j], 0xDEAD);
                CloseHandle(hProcesses[j]);
            } while (--j >= 0);
            free(pTransforms);
            ClearPixelShaders(3);
            if (!upRetError) {// can be set by this thread or any of the temporary worker threads
                upRetError = 1;// out of memory error
            }
            return upRetError;
        }
        j = upTotalCount - 2;
        do {
            EXECUTE_ASSERT(CloseHandle(hProcesses[j]));
        } while (--j >= 0);
    } else {
        CustShadCThreadStatic(pTransforms);
    }
    free(pTransforms);

    if (upRetError) {// can be set by this thread or any of the temporary worker threads
        ClearPixelShaders(3);
    }
    return upRetError;
}

__declspec(nothrow noalias) void CDX9AllocatorPresenter::ClearPixelShaders(unsigned __int8 u8RenderStages)
{
    CAutoLock cRenderLock(&m_csRenderLock);
    if (u8RenderStages & 1) {
        POSITION pos = m_apCustomPixelShaders[0].GetHeadPosition();
        if (pos) {
            do {
                EXTERNALSHADER& ceShader = m_apCustomPixelShaders[0].GetNext(pos);
                if (ceShader.pSrcData) {
                    ULONG u = ceShader.pSrcData->Release();
                    ASSERT(!u);
                    if (ceShader.pPixelShader) {
                        u = ceShader.pPixelShader->Release();
                        ASSERT(!u);
                    }
                } else {
                    ASSERT(!ceShader.pPixelShader);
                }
                if (ceShader.strSrcData) {// note: this function is also called to clean up after failure of SetPixelShaders()
                    // if memory allocation failed in SetPixelShaders(), this pointer is nullptr
                    // other parts of the code, such as the destructor may assume this pointer to always be available
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
                    _aligned_free(ceShader.strSrcData);
#else
                    free(ceShader.strSrcData);
#endif
                }
            } while (pos);
            m_apCustomPixelShaders[0].RemoveAll();
        }
    }
    if (u8RenderStages & 2) {
        POSITION pos = m_apCustomPixelShaders[1].GetHeadPosition();
        if (pos) {
            do {
                EXTERNALSHADER& ceShader = m_apCustomPixelShaders[1].GetNext(pos);
                if (ceShader.pSrcData) {
                    ULONG u = ceShader.pSrcData->Release();
                    ASSERT(!u);
                    if (ceShader.pPixelShader) {
                        u = ceShader.pPixelShader->Release();
                        ASSERT(!u);
                    }
                } else {
                    ASSERT(!ceShader.pPixelShader);
                }
                if (ceShader.strSrcData) {// note: this function is also called to clean up after failure of SetPixelShaders()
                    // if memory allocation failed in SetPixelShaders(), this pointer is nullptr
                    // other parts of the code, such as the destructor may assume this pointer to always be available
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
                    _aligned_free(ceShader.strSrcData);
#else
                    free(ceShader.strSrcData);
#endif
                }
            } while (pos);
            m_apCustomPixelShaders[1].RemoveAll();
        }
    }
}

// retrieves the monitor EDID info

static __declspec(align(128)) wchar_t const gk_awcCodePage437ForEDIDLookup[256] = {// cache line align
    0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, 0x25D8, 0x25CB, 0x0000, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,// 0xA is defined as a string termination character by VESA standards
    0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8, 0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x2302,
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
    0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
    0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
    0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
};

#pragma pack(push, 4)// this directive is used to force pre-padding on this struct to facilitate 16-byte offset alignment
struct PaddingOf12Bytes {__int32 padding[3];};
struct DISPLAY_DEVICEWPadded : PaddingOf12Bytes, DISPLAY_DEVICEW {};
struct PaddingOf8Bytes {__int32 padding[2];};
struct MONITORINFOEXWPadded : PaddingOf8Bytes, MONITORINFOEXW {};
#pragma pack(pop)

__declspec(nothrow noalias noinline) void CDX9AllocatorPresenter::ReadDisplay()
{
    m_szMonitorName[0] = 0;// clear the first character, to make sure the string is signalled as empty for the stats screen

    static_assert(sizeof(MONITORINFOEXWPadded) == sizeof(MONITORINFOEXW) + 8, "struct MONITORINFOEXW or platform settings changed");
    static_assert(!(offsetof(MONITORINFOEXWPadded, szDevice) & 15), "struct MONITORINFOEXW or platform settings changed");
    __declspec(align(16)) MONITORINFOEXWPadded mi;
    mi.cbSize = sizeof(MONITORINFOEX);
    EXECUTE_ASSERT(GetMonitorInfoW(m_hCurrentMonitor, static_cast<MONITORINFOEXW*>(&mi)));
#if _M_X64// x64 needs to preserve SSE registers anyway
    __m128i x0 = _mm_load_si128(reinterpret_cast<__m128i*>(mi.szDevice));
    __m128i x1 = _mm_load_si128(reinterpret_cast<__m128i*>(mi.szDevice) + 1);
    __m128i x2 = _mm_load_si128(reinterpret_cast<__m128i*>(mi.szDevice) + 2);
    __m128i x3 = _mm_load_si128(reinterpret_cast<__m128i*>(mi.szDevice) + 3);
#endif

    static_assert(sizeof(DISPLAY_DEVICEWPadded) == sizeof(DISPLAY_DEVICEW) + 12, "struct DISPLAY_DEVICEW or platform settings changed");
    static_assert(!(offsetof(DISPLAY_DEVICEWPadded, DeviceName) & 15), "struct DISPLAY_DEVICEW or platform settings changed");
    __declspec(align(16)) DISPLAY_DEVICEWPadded dd;
    dd.cb = sizeof(DISPLAY_DEVICEW);

    DWORD dwDevNum = 0;
    for (;;) {
        if (!EnumDisplayDevicesW(nullptr, dwDevNum, static_cast<DISPLAY_DEVICEW*>(&dd), 0)) {
            ASSERT(0);// device not found
            break;
        }

        if ((dd.StateFlags & DISPLAY_DEVICE_ACTIVE) && !(dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)) {
#if _M_IX86_FP == 1// inverse of: SSE2 code, don't use on SSE builds, works correctly for x64
            if (CPUGetEnabledExtensions() & CPUF_SUPPORTS_SSE2)// SSE2/SSE4.1 code path
#endif
            {
#ifndef _M_X64
                __m128i x0 = _mm_load_si128(reinterpret_cast<__m128i*>(mi.szDevice));
                __m128i x1 = _mm_load_si128(reinterpret_cast<__m128i*>(mi.szDevice) + 1);
                __m128i x2 = _mm_load_si128(reinterpret_cast<__m128i*>(mi.szDevice) + 2);
                __m128i x3 = _mm_load_si128(reinterpret_cast<__m128i*>(mi.szDevice) + 3);
#endif
                __m128i x4 = _mm_load_si128(reinterpret_cast<__m128i*>(dd.DeviceName));
                __m128i x5 = _mm_load_si128(reinterpret_cast<__m128i*>(dd.DeviceName) + 1);
                __m128i x6 = _mm_load_si128(reinterpret_cast<__m128i*>(dd.DeviceName) + 2);
                __m128i x7 = _mm_load_si128(reinterpret_cast<__m128i*>(dd.DeviceName) + 3);
                x4 = _mm_xor_si128(x4, x0);
                x5 = _mm_xor_si128(x5, x1);
                x6 = _mm_xor_si128(x6, x2);
                x7 = _mm_xor_si128(x7, x3);
                x4 = _mm_or_si128(x4, x5);
                x6 = _mm_or_si128(x6, x7);
                x4 = _mm_or_si128(x4, x6);
                if (CPUGetEnabledExtensions() & CPUF_SUPPORTS_SSE41) {// SSE4.1 code path
                    if (_mm_testz_si128(x4, x4)) {
                        goto DeviceFoundEnterLoop;
                    }
                } else {// SSE2 code path
                    if (0xFFFF == _mm_movemask_epi8(_mm_cmpeq_epi32(x4, _mm_setzero_si128()))) {
                        goto DeviceFoundEnterLoop;
                    }
                }
            }
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
            goto TryNextDevice;
#else
            else if (reinterpret_cast<__int64*>(dd.DeviceName)[0] == reinterpret_cast<__int64*>(mi.szDevice)[0]// SSE code path
                     && reinterpret_cast<__int64*>(dd.DeviceName)[1] == reinterpret_cast<__int64*>(mi.szDevice)[1]
                     && reinterpret_cast<__int64*>(dd.DeviceName)[2] == reinterpret_cast<__int64*>(mi.szDevice)[2]
                     && reinterpret_cast<__int64*>(dd.DeviceName)[3] == reinterpret_cast<__int64*>(mi.szDevice)[3]
                     && reinterpret_cast<__int64*>(dd.DeviceName)[4] == reinterpret_cast<__int64*>(mi.szDevice)[4]
                     && reinterpret_cast<__int64*>(dd.DeviceName)[5] == reinterpret_cast<__int64*>(mi.szDevice)[5]
                     && reinterpret_cast<__int64*>(dd.DeviceName)[6] == reinterpret_cast<__int64*>(mi.szDevice)[6]
                     && reinterpret_cast<__int64*>(dd.DeviceName)[7] == reinterpret_cast<__int64*>(mi.szDevice)[7])
#endif
            {
DeviceFoundEnterLoop:
                EXECUTE_ASSERT(EnumDisplayDevicesW(mi.szDevice, 0, static_cast<DISPLAY_DEVICEW*>(&dd), 0));
                size_t len = wcslen(dd.DeviceID);
                wchar_t* szDeviceIDshort = dd.DeviceID + len - 43;// fixed at 43 characters

                HKEY hKey0;
                static wchar_t const gk_szRegCcsEnumDisplay[] = L"SYSTEM\\CurrentControlSet\\Enum\\DISPLAY\\";
                LSTATUS ls = RegOpenKeyExW(HKEY_LOCAL_MACHINE, gk_szRegCcsEnumDisplay, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hKey0);
                if (ls == ERROR_SUCCESS) {
                    DWORD i = 0;
                    for (;;) {// iterate over the child keys
                        DWORD cbName = _countof(dd.DeviceKey);
                        ls = RegEnumKeyExW(hKey0, i, dd.DeviceKey, &cbName, nullptr, nullptr, nullptr, nullptr);
                        if (ls == ERROR_NO_MORE_ITEMS) {
                            break;
                        }

                        if (ls == ERROR_SUCCESS) {
                            static_assert((offsetof(DISPLAY_DEVICEW, DeviceKey) - offsetof(DISPLAY_DEVICEW, DeviceName)) >= 2 * 257, "struct DISPLAY_DEVICEW or platform settings changed");// there's plenty of space inside the structure to accomodate the maximum registry key name length
                            memcpy(dd.DeviceName, gk_szRegCcsEnumDisplay, sizeof(gk_szRegCcsEnumDisplay) - 2);// chop off null character
                            memcpy(dd.DeviceName + _countof(gk_szRegCcsEnumDisplay) - 1, dd.DeviceKey, (cbName << 1) + 2);
                            wchar_t* pEnd0 = dd.DeviceName + _countof(gk_szRegCcsEnumDisplay) - 1 + cbName;
                            HKEY hKey1;
                            ls = RegOpenKeyExW(HKEY_LOCAL_MACHINE, dd.DeviceName, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hKey1);

                            if (ls == ERROR_SUCCESS) {
                                DWORD j = 0;
                                for (;;) {// iterate over the grandchild keys
                                    cbName = _countof(dd.DeviceKey);
                                    ls = RegEnumKeyExW(hKey1, j, dd.DeviceKey, &cbName, nullptr, nullptr, nullptr, nullptr);
                                    if (ls == ERROR_NO_MORE_ITEMS) {
                                        break;
                                    }

                                    if (ls == ERROR_SUCCESS) {
                                        *pEnd0 = L'\\';
                                        memcpy(pEnd0 + 1, dd.DeviceKey, (cbName << 1) + 2);
                                        wchar_t* pEnd1 = pEnd0 + 1 + cbName;

                                        static wchar_t const szTDriverKeyN[] = L"Driver";
                                        cbName = sizeof(dd.DeviceKey);// re-use it here
                                        ls = RegGetValueW(HKEY_LOCAL_MACHINE, dd.DeviceName, szTDriverKeyN, RRF_RT_REG_SZ, nullptr, dd.DeviceKey, &cbName);

                                        if (ls == ERROR_SUCCESS) {
                                            if (!wcscmp(szDeviceIDshort, dd.DeviceKey)) {
                                                static wchar_t const szTDevParKeyN[] = L"\\Device Parameters";
                                                memcpy(pEnd1, szTDevParKeyN, sizeof(szTDevParKeyN));

                                                static wchar_t const szkEDIDKeyN[] = L"EDID";
                                                cbName = sizeof(dd.DeviceKey);// 256, perfectly suited to receive a copy of the 128 or 256 bytes of (E-)EDID data
                                                ls = RegGetValueW(HKEY_LOCAL_MACHINE, dd.DeviceName, szkEDIDKeyN, RRF_RT_REG_BINARY, nullptr, dd.DeviceKey, &cbName);
                                                if ((ls == ERROR_SUCCESS) && (cbName > 127)) {
                                                    unsigned __int8* EDIDdata = reinterpret_cast<unsigned __int8*>(dd.DeviceKey);
                                                    // memo: bytes 25 to 34 contain the default chromaticity coordinates

                                                    // pixel clock in 10 kHz units (0.01–655.35 MHz)
                                                    unsigned __int16 u16PixelClock = *reinterpret_cast<unsigned __int16*>(EDIDdata + 54);
                                                    if (u16PixelClock) {// if the descriptor for pixel clock is 0, the descriptor block is invalid
                                                        // horizontal active pixels
                                                        m_u16MonitorHorRes = (static_cast<unsigned __int16>(EDIDdata[58] & 0xF0) << 4) | EDIDdata[56];
                                                        // horizontal blanking pixels
                                                        // unsigned __int16 u16HorizontalBlanking = (static_cast<unsigned __int16>(EDIDdata[58] & 0x0F) << 8) | EDIDdata[57];
                                                        // vertical active pixels
                                                        m_u16MonitorVerRes = (static_cast<unsigned __int16>(EDIDdata[61] & 0xF0) << 4) | EDIDdata[59];
                                                        // vertical blanking lines
                                                        // unsigned __int16 u16VerticalBlanking = (static_cast<unsigned __int16>(EDIDdata[61] & 0x0F) << 8) | EDIDdata[60];
                                                        // horizontal sync offset pixels
                                                        // unsigned __int16 u16HorizontalSyncOffset = (static_cast<unsigned __int16>(EDIDdata[65] & 0xC0) << 2) | EDIDdata[62];
                                                        // horizontal sync pulse width pixels
                                                        // unsigned __int16 u16HorizontalSyncPulseWidth = (static_cast<unsigned __int16>(EDIDdata[65] & 0x30) << 4) | EDIDdata[63];
                                                        // vertical sync offset lines
                                                        // unsigned __int16 u16VerticalSyncOffset = (static_cast<unsigned __int16>(EDIDdata[65] & 0x0C) << 2) | ((EDIDdata[64] & 0xF0) >> 4);
                                                        // vertical sync pulse width lines
                                                        // unsigned __int16 u16VerticalSyncPulseWidth = (static_cast<unsigned __int16>(EDIDdata[65] & 0x03) << 4) | (EDIDdata[64] & 0x0F);

                                                        // physical display width in mm
                                                        m_u16mmMonitorWidth = (static_cast<unsigned __int16>(EDIDdata[68] & 0xF0) << 4) | EDIDdata[66];
                                                        // physical display height in mm
                                                        m_u16mmMonitorHeight = (static_cast<unsigned __int16>(EDIDdata[68] & 0x0F) << 8) | EDIDdata[67];

                                                        // validate and identify extra descriptor blocks
                                                        // memo: descriptor block identifier 0xFB is used for additional white point data
                                                        ptrdiff_t k = 12;
                                                        if (!*reinterpret_cast<unsigned __int16*>(EDIDdata + 72) && (EDIDdata[75] == 0xFC)) {// descriptor block 2, the first 16 bits must be zero, else the descriptor contains detailed timing data, identifier 0xFC is used for monitor name
                                                            do {
                                                                m_szMonitorName[k] = gk_awcCodePage437ForEDIDLookup[EDIDdata[77 + k]];
                                                            } while (--k >= 0);
                                                        } else if (!*reinterpret_cast<unsigned __int16*>(EDIDdata + 90) && (EDIDdata[93] == 0xFC)) {// descriptor block 3
                                                            do {
                                                                m_szMonitorName[k] = gk_awcCodePage437ForEDIDLookup[EDIDdata[95 + k]];
                                                            } while (--k >= 0);
                                                        } else if (!*reinterpret_cast<unsigned __int16*>(EDIDdata + 108) && (EDIDdata[111] == 0xFC)) {// descriptor block 4
                                                            do {
                                                                m_szMonitorName[k] = gk_awcCodePage437ForEDIDLookup[EDIDdata[113 + k]];
                                                            } while (--k >= 0);
                                                        } else {// monitor name not found, copy the first 13 characters of the GDI display name, character 14 needs to stay 0
                                                            // aligned memory copy, no need for a loop here
#if _M_X64// x64 needs to preserve SSE registers anyway
                                                            x2 = _mm_shuffle_epi32(x1, _MM_SHUFFLE(3, 2, 3, 2));
                                                            _mm_store_si128(reinterpret_cast<__m128i*>(m_szMonitorName), x0);
                                                            __int32 i32pn = _mm_cvtsi128_si32(x2);
                                                            _mm_storel_epi64(reinterpret_cast<__m128i*>(m_szMonitorName) + 1, x1);
                                                            m_szMonitorName[12] = static_cast<wchar_t>(i32pn);
#else// all SSE registers are volatile, re-load data from mi.szDevice on the stack
                                                            __m128 xn = _mm_load_ps(reinterpret_cast<float*>(mi.szDevice));
                                                            __int64 i64n = reinterpret_cast<__int64*>(mi.szDevice)[2];
                                                            wchar_t wcn = mi.szDevice[12];
                                                            _mm_store_ps(reinterpret_cast<float*>(m_szMonitorName), xn);
                                                            reinterpret_cast<__int64*>(m_szMonitorName)[2] = i64n;
                                                            m_szMonitorName[12] = wcn;
#endif
                                                        }
                                                    }
                                                    EXECUTE_ASSERT(ERROR_SUCCESS == RegCloseKey(hKey1));
                                                    EXECUTE_ASSERT(ERROR_SUCCESS == RegCloseKey(hKey0));
                                                    return;
                                                }
                                            }
                                        }
                                    }
                                    ++j;
                                }
                                EXECUTE_ASSERT(ERROR_SUCCESS == RegCloseKey(hKey1));
                            }
                        }
                        ++i;
                    }
                    EXECUTE_ASSERT(ERROR_SUCCESS == RegCloseKey(hKey0));
                }
                break;// device names are unique; once the device has been found, break the loop
            }
        }
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
TryNextDevice:
#endif
        ++dwDevNum;
    }
}

// display refresh rate reading functions

__declspec(nothrow noalias noinline) bool CDX9AllocatorPresenter::GenericForExMode()
{
    TRACE(L"Video renderer attempting to use the generic Ex mode method to read the display refresh rate\n");
    DWORD dwPhysicalMonitorArrayCount;
    HRESULT hr = m_fnGetNumberOfPhysicalMonitorsFromHMONITOR(m_hCurrentMonitor, &dwPhysicalMonitorArrayCount);// will return more than 1 in case of a monitor-cloned setup
    if (FAILED(hr)) {
        ASSERT(0);
        return false;
    }

    PHYSICAL_MONITOR* phPhysicalMonitorArray = reinterpret_cast<PHYSICAL_MONITOR*>(malloc(dwPhysicalMonitorArrayCount * sizeof(PHYSICAL_MONITOR)));
    if (!phPhysicalMonitorArray) {
        ASSERT(0);
        return false;
    }
    hr = m_fnGetPhysicalMonitorsFromHMONITOR(m_hCurrentMonitor, dwPhysicalMonitorArrayCount, phPhysicalMonitorArray);
    if (FAILED(hr)) {
        ASSERT(0);
        free(phPhysicalMonitorArray);
        return false;
    }

    bool bReturnValue = false;
    MC_TIMING_REPORT trCurr;
    _BOOL bowu = m_fnGetTimingReport(phPhysicalMonitorArray->hPhysicalMonitor, &trCurr);// the first in the array is always the main monitor, that's what we are looking for
    if (!bowu) {// no ASSERT here, sometimes querying the timing data just doesn't work
#ifdef _DEBUG
        DWORD dwErr = GetLastError();
        LPWSTR pstrMsgBuf;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, dwErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&pstrMsgBuf), 0, nullptr);
        TRACE(L"Video renderer GetTimingReport() failed the first try with error 0x%x: %s, the renderer is giving the driver half a second and will try again\n", dwErr, pstrMsgBuf);
        LocalFree(pstrMsgBuf);
#endif
        Sleep(500);// remember, the I²C bus is an older type of bus interface, it really needs this much recovery time - I tried smaller values at first, of course
        bowu = m_fnGetTimingReport(phPhysicalMonitorArray->hPhysicalMonitor, &trCurr);
        if (!bowu) {
#ifdef _DEBUG
            dwErr = GetLastError();
            LPWSTR pstrMsgBuf;
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, dwErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&pstrMsgBuf), 0, nullptr);
            TRACE(L"Video renderer GetTimingReport() failed the second try with error 0x%x: %s, the renderer is again giving the driver half a second and will try again\n", dwErr, pstrMsgBuf);
            LocalFree(pstrMsgBuf);
#endif
            Sleep(500);
            bowu = m_fnGetTimingReport(phPhysicalMonitorArray->hPhysicalMonitor, &trCurr);
            if (!bowu) {
#ifdef _DEBUG
                dwErr = GetLastError();
                LPWSTR pstrMsgBuf;
                FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, dwErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&pstrMsgBuf), 0, nullptr);
                TRACE(L"Video renderer GetTimingReport() failed the third try with error 0x%x: %s\n", dwErr, pstrMsgBuf);
                LocalFree(pstrMsgBuf);
#endif
                goto NoReportFound;
            }
        }
    }

    {
        // note: the "Get Timing Report & Timing Message" DDC/CI command outputs 16 bits of valid data for each of these two values
        DWORD dwFh = trCurr.dwHorizontalFrequencyInHZ;
        DWORD dwFv = trCurr.dwVerticalFrequencyInHZ;

        // reports of all 0 and all 1 are invalid
        if (!dwFh) {
            TRACE(L"Video renderer Generic Ex mode - timing report failed: the reported horizontal display frequency is 0\n");
            goto NoReportFound;
        } else if ((dwFh & 0xFFFF) == 0xFFFF) {
            TRACE(L"Video renderer Generic Ex mode - timing report failed: the reported horizontal display frequency is invalid\n");
            goto NoReportFound;
        } else if (!dwFv) {
            TRACE(L"Video renderer Generic Ex mode - timing report failed: the reported vertical display frequency is 0\n");
            goto NoReportFound;
        } else if ((dwFv & 0xFFFF) == 0xFFFF) {
            TRACE(L"Video renderer Generic Ex mode - timing report failed: the reported vertical display frequency is invalid\n");
            goto NoReportFound;
        }

#ifdef _DEBUG
        static wchar_t const szVHkStFactor[] = L"1";
        static wchar_t const szVsFactor[] = L".001";
        static wchar_t const szVmFactor[] = L".01";
        static wchar_t const szVlFactor[] = L".1";
        wchar_t const* szVerFactor = szVHkStFactor;
#endif
        // the standard specifies 0.01 Hz units, but some manufacturers don't set this right
        // 23 Hz is the lower than the lowest common vertical refresh rate of 24/1.001 Hz
        // 230 Hz is a lot more than the common display connectors can carry with normal video resolutions
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
        __m128d xFv  = _mm_setzero_pd();
        xFv = _mm_cvtsi32_sd(xFv, dwFv);// 1 Hz units
        if (dwFv >= 23000) {// 0.001 Hz units
            DEBUG_ONLY(szVerFactor = szVsFactor);
            static __declspec(align(16)) double const dkFvMs = 0.001;
            xFv = _mm_mul_sd(xFv, *reinterpret_cast<__m128d const*>(&dkFvMs));
        } else if (dwFv >= 2300) {// 0.01 Hz units
            DEBUG_ONLY(szVerFactor = szVmFactor);
            static __declspec(align(16)) double const dkFvMm = 0.01;
            xFv = _mm_mul_sd(xFv, *reinterpret_cast<__m128d const*>(&dkFvMm));
        } else if (dwFv >= 230) {// 0.1 Hz units
            DEBUG_ONLY(szVerFactor = szVlFactor);
            static __declspec(align(16)) double const dkFvMl = 0.1;
            xFv = _mm_mul_sd(xFv, *reinterpret_cast<__m128d const*>(&dkFvMl));
        }
#else
        double dFv = static_cast<double>(static_cast<__int32>(dwFv));// 1 Hz units, the standard converter only does a proper job with signed values
        if (dwFv >= 23000) {// 0.001 Hz units
            DEBUG_ONLY(szVerFactor = szVsFactor);
            dFv *= 0.001;
        } else if (dwFv >= 2300) {// 0.01 Hz units
            DEBUG_ONLY(szVerFactor = szVmFactor);
            dFv *= 0.01;
        } else if (dwFv >= 230) {// 0.1 Hz units
            DEBUG_ONLY(szVerFactor = szVlFactor);
            dFv *= 0.1;
        }
#endif
#ifdef _DEBUG
        static wchar_t const szHlFactor[] = L"1000";
        static wchar_t const szHmFactor[] = L"100";
        static wchar_t const szHsFactor[] = L"10";
        wchar_t const* szHorFactor = szVHkStFactor;
#endif
        // the standard specifies 1 Hz units, some manufacturers don't set this right (independent from the data for vertical refresh rate)
        // 15 kHz is the old FM/NTSC audio/PAL&SECAM audio bandwidth (NTSC video is at 15750 Hz B/W or 15750/1.001 color, PAL&SECAM video is at 15625 Hz), it's rather unlikely that a manufacturer would use anything lower than that
        // 150 kHz can be exceeded easily; We just assume that if the horizontal frequency is equal or larger than 15000, it is set correctly in 1 Hz units
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
        __m128d xFh  = _mm_setzero_pd();
        xFh = _mm_cvtsi32_sd(xFh, dwFh);// 1 Hz units
        if (dwFh < 150) {// 1000 Hz units
            DEBUG_ONLY(szHorFactor = szHlFactor);
            static __declspec(align(16)) double const dkFhMl = 1000.0;
            xFh = _mm_mul_sd(xFh, *reinterpret_cast<__m128d const*>(&dkFhMl));
        } else if (dwFv < 1500) {// 100 Hz units
            DEBUG_ONLY(szHorFactor = szHmFactor);
            static __declspec(align(16)) double const dkFhMm = 100.0;
            xFh = _mm_mul_sd(xFh, *reinterpret_cast<__m128d const*>(&dkFhMm));
        } else if (dwFv < 15000) {// 10 Hz units
            DEBUG_ONLY(szHorFactor = szHsFactor);
            static __declspec(align(16)) double const dkFhMs = 10.0;
            xFh = _mm_mul_sd(xFh, *reinterpret_cast<__m128d const*>(&dkFhMs));
        }
#else
        double dFh = static_cast<double>(static_cast<__int32>(dwFh));// 1 Hz units, the standard converter only does a proper job with signed values
        if (dwFh < 150) {// 1000 Hz units
            DEBUG_ONLY(szHorFactor = szHlFactor);
            dFh *= 1000.0;
        } else if (dwFh < 1500) {// 100 Hz units
            DEBUG_ONLY(szHorFactor = szHmFactor);
            dFh *= 100.0;

        } else if (dwFh < 15000) {// 10 Hz units
            DEBUG_ONLY(szHorFactor = szHsFactor);
            dFh *= 10.0;
        }
#endif
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
        static __declspec(align(16)) __int64 const aki64LargeExp[2] = {0x4330000000000000, 0x4330000000000000};// 1<<52
        __m128d xLargeExp = _mm_load_pd(reinterpret_cast<double const*>(aki64LargeExp));
        __m128d xRate = xFh;
        __m128d xFhCopy = xFh;
        xFh = _mm_div_sd(xFh, xFv);// horizontal / vertical frequency yields the total scan line count, which we round, as this is always a whole amount

        // quick rounding of a positive number that is much smaller than the maximum double size
        ASSERT(_MM_GET_ROUNDING_MODE() == _MM_ROUND_NEAREST);
        xFh = _mm_add_sd(xFh, xLargeExp);
        xFh = _mm_sub_sd(xFh, xLargeExp);

        // pack two vectors as {a, b} and {b, a} before the SIMD division
        xRate = _mm_unpacklo_pd(xRate, xFh);
        xFh = _mm_unpacklo_pd(xFh, xFhCopy);

        xRate = _mm_div_pd(xRate, xFh);// this is generally more precise than just dFv, because of the scan line counting
        _mm_store_sd(&m_dDetectedScanlinesPerFrame, xFh);
        _mm_store_sd(&m_dReferenceRefreshRate, xRate);
        _mm_store_pd(&m_dDetectedRefreshRate, xRate);// also stores m_dDetectedRefreshTime
        DEBUG_ONLY(TRACE(L"Video renderer Generic Ex mode - timing report succeeded: %f Hz, %u scan lines, raw vf %hu * %s, raw hf %hu * %s\n", _mm_cvtsd_f64(xRate), _mm_cvtsd_si32(xFh), dwFv, szVerFactor, dwFh, szHorFactor));
#else
        double dTotalScanLineCount = floor(dFh / dFv + 0.5);// horizontal / vertical frequency yields the total scan line count, which we round, as this is always a whole amount
        double dRefreshRate = dFh / dTotalScanLineCount;// this is generally more precise than just dFv, because of the scan line counting
        m_dDetectedRefreshRate = m_dReferenceRefreshRate = dRefreshRate;
        m_dDetectedScanlinesPerFrame = dTotalScanLineCount;
        m_dDetectedRefreshTime = dTotalScanLineCount / dFh;
        DEBUG_ONLY(TRACE(L"Video renderer Generic Ex mode - timing report succeeded: %f Hz, %u scan lines, raw vf %hu * %s, raw hf %hu * %s\n", dRefreshRate, static_cast<__int32>(dTotalScanLineCount), dwFv, szVerFactor, dwFh, szHorFactor));
#endif
        bReturnValue = true;
    }

NoReportFound:
    if (!bReturnValue) {
        TRACE(L"Video renderer Generic Ex mode - timing report failed, so reverted to the old method\n");
    }
    EXECUTE_ASSERT(m_fnDestroyPhysicalMonitors(dwPhysicalMonitorArrayCount, phPhysicalMonitorArray));
    free(phPhysicalMonitorArray);
    return bReturnValue;
}

// AMD specific (ADL library)

// Memory allocation function required by ADL

#ifdef _WIN64
#define ADL_Main_Memory_Alloc malloc// the ADL_MAIN_MALLOC_CALLBACK declaration is modified, so it can take malloc() directly
#else
static __declspec(nothrow noalias restrict) void* __stdcall ADL_Main_Memory_Alloc(int iSize)// ADL_MAIN_MALLOC_CALLBACK is not directly comptible with malloc() under x86-32
{
    void* pBuffer = malloc(iSize);
    ASSERT(pBuffer);
    return pBuffer;
}
#endif

__declspec(nothrow noalias noinline) bool CDX9AllocatorPresenter::SpecificForAMD()
{
    TRACE(L"Video renderer attempting to use the AMD ADL library to read the display refresh rate\n");
    HINSTANCE hADL = LoadLibraryW(L"atiadlxx.dll");
#ifndef _WIN64
    if (!hADL) {
        // a 32 bit calling application on 64 bit OS will fail on the previous LoadLibraryW()
        // try to load the 32 bit library (atiadlxy.dll) instead
        hADL = LoadLibraryW(L"atiadlxy.dll");
#endif
        if (!hADL) {
            ASSERT(0);
            return false;
        }
#ifndef _WIN64
    }
#endif
    {
        // import functions from atiadlx?.dll
        uintptr_t pModule = reinterpret_cast<uintptr_t>(hADL);// just a named alias
        IMAGE_DOS_HEADER const* pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER const*>(pModule);
        IMAGE_NT_HEADERS const* pNTHeader = reinterpret_cast<IMAGE_NT_HEADERS const*>(pModule + static_cast<size_t>(static_cast<ULONG>(pDOSHeader->e_lfanew)));
        IMAGE_EXPORT_DIRECTORY const* pEAT = reinterpret_cast<IMAGE_EXPORT_DIRECTORY const*>(pModule + static_cast<size_t>(pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress));
        uintptr_t pAONbase = pModule + static_cast<size_t>(pEAT->AddressOfNames);
        uintptr_t pAONObase = pModule + static_cast<size_t>(pEAT->AddressOfNameOrdinals);
        uintptr_t pAOFbase = pModule + static_cast<size_t>(pEAT->AddressOfFunctions);
        DWORD dwLoopCount = pEAT->NumberOfNames - 1;
        {
            __declspec(align(8)) static char const kszFunc[] = "ADL_Adapter_NumberOfAdapters_Get";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
            ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
            for (;;) {
                unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 24) == *reinterpret_cast<__int64 const*>(kszFunc + 24)
                        && kszName[32] == kszFunc[32]) {// note that this part must compare zero end inclusive
                    // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                    break;
                } else if (--i < 0) {
                    ASSERT(0);
                    return false;
                }
            }
            unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
            unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
            AMD.m_fnADL_Adapter_NumberOfAdapters_Get = reinterpret_cast<ADL_Adapter_NumberOfAdapters_GetPtr>(pModule + static_cast<size_t>(u32AOF));
        }
        {
            __declspec(align(8)) static char const kszFunc[] = "ADL_Adapter_AdapterInfo_Get";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
            ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
            for (;;) {
                unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)
                        && *reinterpret_cast<__int32 __unaligned const*>(kszName + 24) == *reinterpret_cast<__int32 const*>(kszFunc + 24)) {// note that this part must compare zero end inclusive
                    // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                    break;
                } else if (--i < 0) {
                    ASSERT(0);
                    return false;
                }
            }
            unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
            unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
            AMD.m_fnADL_Adapter_AdapterInfo_Get = reinterpret_cast<ADL_Adapter_AdapterInfo_GetPtr>(pModule + static_cast<size_t>(u32AOF));
        }
        {
            __declspec(align(8)) static char const kszFunc[] = "ADL_Display_DisplayInfo_Get";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
            ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
            for (;;) {
                unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)
                        && *reinterpret_cast<__int32 __unaligned const*>(kszName + 24) == *reinterpret_cast<__int32 const*>(kszFunc + 24)) {// note that this part must compare zero end inclusive
                    // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                    break;
                } else if (--i < 0) {
                    ASSERT(0);
                    return false;
                }
            }
            unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
            unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
            AMD.m_fnADL_Display_DisplayInfo_Get = reinterpret_cast<ADL_Display_DisplayInfo_GetPtr>(pModule + static_cast<size_t>(u32AOF));
        }
        {
            __declspec(align(8)) static char const kszFunc[] = "ADL_Display_DDCBlockAccess_Get";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
            ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
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
                    return false;
                }
            }
            unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
            unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
            AMD.m_fnADL_Display_DDCBlockAccess_Get = reinterpret_cast<ADL_Display_DDCBlockAccess_GetPtr>(pModule + static_cast<size_t>(u32AOF));
        }
        {
            __declspec(align(8)) static char const kszFunc[] = "ADL_Main_Control_Destroy";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
            ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
            for (;;) {
                unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)
                        && kszName[24] == kszFunc[24]) {// note that this part must compare zero end inclusive
                    // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                    break;
                } else if (--i < 0) {
                    ASSERT(0);
                    return false;
                }
            }
            unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
            unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
            AMD.m_fnADL_Main_Control_Destroy = reinterpret_cast<ADL_Main_Control_DestroyPtr>(pModule + static_cast<size_t>(u32AOF));
        }
        {
            __declspec(align(8)) static char const kszFunc[] = "ADL_Main_Control_Create";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
            ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
            for (;;) {
                unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int64 const*>(kszFunc + 8)
                        && *reinterpret_cast<__int64 __unaligned const*>(kszName + 16) == *reinterpret_cast<__int64 const*>(kszFunc + 16)) {// note that this part must compare zero end inclusive
                    // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                    break;
                } else if (--i < 0) {
                    ASSERT(0);
                    return false;
                }
            }
            unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
            unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
            AMD.m_fnADL_Main_Control_Create = reinterpret_cast<ADL_Main_Control_CreatePtr>(pModule + static_cast<size_t>(u32AOF));
        }
    }

    // Initialize ADL, only retrieve adapter information only for adapters that are physically present and enabled in the system
    if (ADL_OK != AMD.m_fnADL_Main_Control_Create(ADL_Main_Memory_Alloc, 1)) {
        ASSERT(0);
        return false;
    }

    // Obtain the number of adapters for the system
    int iNumberAdapters;
    if ((ADL_OK != AMD.m_fnADL_Adapter_NumberOfAdapters_Get(&iNumberAdapters)) || (0 >= iNumberAdapters)) {
        ASSERT(0);
        return false;
    }

    // Get the AdapterInfo structure for all adapters in the system
    AdapterInfo* pAdapterInfo = reinterpret_cast<AdapterInfo*>(malloc(sizeof(AdapterInfo) * iNumberAdapters));
    if (ADL_OK != AMD.m_fnADL_Adapter_AdapterInfo_Get(pAdapterInfo, sizeof(AdapterInfo) * iNumberAdapters)) {
        ASSERT(0);
        return false;
    }

    bool bReturnValue = false;
    unsigned __int8 au8Output[256];
    ptrdiff_t i = iNumberAdapters - 1;
    do {
        int iNumDisplays;
        ADLDisplayInfo* pAdlDisplayInfo = nullptr;
        if (ADL_OK != AMD.m_fnADL_Display_DisplayInfo_Get(pAdapterInfo[i].iAdapterIndex, &iNumDisplays, &pAdlDisplayInfo, 0)) {
            continue;
        }

        // 16-byte alignment is guaranteed for m_szGDIDisplayDeviceName, 8-byte (x86-32) or better (newer systems) alignment is guaranteed for pAdapterInfo[i].strDisplayName through malloc(), combined with the organization of the struct
        if ((reinterpret_cast<__int64*>(m_szGDIDisplayDeviceName)[0] != reinterpret_cast<__int64*>(pAdapterInfo[i].strDisplayName)[0])
                || (reinterpret_cast<__int64*>(m_szGDIDisplayDeviceName)[1] != reinterpret_cast<__int64*>(pAdapterInfo[i].strDisplayName)[1])
                || (reinterpret_cast<__int64*>(m_szGDIDisplayDeviceName)[2] != reinterpret_cast<__int64*>(pAdapterInfo[i].strDisplayName)[2])
                || (reinterpret_cast<__int64*>(m_szGDIDisplayDeviceName)[3] != reinterpret_cast<__int64*>(pAdapterInfo[i].strDisplayName)[3])) {
            free(pAdlDisplayInfo);
            continue;
        }

        int iAdapterIndex = pAdapterInfo[i].iAdapterIndex;
        ptrdiff_t j = iNumDisplays;
        while (--j >= 0) {// the first one could be 0
            // use the display only if it's connected and mapped (iDisplayInfoValue: bit 0 and 1)
            int iDisplayInfoValue = pAdlDisplayInfo[j].iDisplayInfoValue;
            if (!(iDisplayInfoValue & ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED)
                    || !(iDisplayInfoValue & ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED)) {
                continue;
            }

            // use the display only if it's mapped to this adapter
            if (iAdapterIndex != pAdlDisplayInfo[j].displayID.iDisplayLogicalAdapterIndex) {
                continue;
            }
            int iDisplayIndex = pAdlDisplayInfo[j].displayID.iDisplayLogicalIndex;

            unsigned __int8 u8RetryCount = 3;// three attempts before failing
RetryGetReport:
            int iOutputsize = 256;
            static unsigned __int8 const aku8Input[] = {0x6E, 0x51, 0x81, 0x07, 0x6E ^ 0x51 ^ 0x81 ^ 0x07};// command: "Get Timing Report & Timing Message", checksum XOR at end
            if (ADL_OK == AMD.m_fnADL_Display_DDCBlockAccess_Get(iAdapterIndex, iDisplayIndex, 0, 0, sizeof(aku8Input), aku8Input, &iOutputsize, au8Output)) {
                DWORD dwFh, dwFv;
                {
                    // scan for the reply message
                    unsigned __int8* pOutI = au8Output;
                    unsigned __int8 k = static_cast<unsigned __int8>(iOutputsize - 7);// the following method needs the 8 bytes in the reply message (unsigned counter breaks at 0)
                    do {
                        unsigned __int32 u32rawb = *reinterpret_cast<unsigned __int32 __unaligned*>(pOutI);
                        if ((u32rawb & 0xFFFFFF) == 0x4E066E) {// 0x6E: Source address, 0x06: Timing message command, 0x4E: Timing message op code, next byte can vary
                            unsigned __int8 u8SS = u32rawb >> 24;
                            if (u8SS & (0xBC << 24)) {// Timing Status byte; Bit 7 = 1: Sync.Freq. out of range, Bit 5 to 2: Reserved, shall be set to 0
                                goto NoValidMessageInReport;
                            }
                            // verify checksum
                            // the message is 9 bytes in total, 4 bytes have already been read
                            unsigned __int8 u8HH = pOutI[4], u8HL = pOutI[5], u8VH = pOutI[6], u8VL = pOutI[7], u8CHK50 = pOutI[8];
                            unsigned __int8 u8CS = 0x50 ^ 0x6E ^ 0x06 ^ 0x4E ^ u8SS ^ u8HH ^ u8HL ^ u8VH ^ u8VL;// the checksum XOR at the end is computed using the 0x50 virtual host address
                            if (u8CS != u8CHK50) {
                                goto NoValidMessageInReport;
                            }
                            // load big endian data
                            dwFh = static_cast<unsigned __int32>(u8HH) << 8 | u8HL;
                            dwFv = static_cast<unsigned __int32>(u8VH) << 8 | u8VL;
                            goto ReportFound;
                        }
                        ++pOutI;
                    } while (--k);
NoValidMessageInReport:
                    if (--u8RetryCount) {
                        TRACE(L"Video renderer ADL - timing report failed the &s try: no valid message in the reply from the display device, the renderer is giving the driver half a second and will try again\n", (u8RetryCount == 2) ? L"first" : L"second");
                        Sleep(500);
                        goto RetryGetReport;
                    }
                    TRACE(L"Video renderer ADL - timing report failed the third try: no valid message in the reply from the display device\n");
                    goto NoReportFound;
                }
ReportFound: {
                    // reports of all 0 and all 1 are invalid
                    if (!dwFh) {
                        TRACE(L"Video renderer ADL - timing report failed: the reported horizontal display frequency is 0\n");
                        goto NoReportFound;
                    } else if (dwFh == 0xFFFF) {
                        TRACE(L"Video renderer ADL - timing report failed: the reported horizontal display frequency is invalid\n");
                        goto NoReportFound;
                    } else if (!dwFv) {
                        TRACE(L"Video renderer ADL - timing report failed: the reported vertical display frequency is 0\n");
                        goto NoReportFound;
                    } else if (dwFv == 0xFFFF) {
                        TRACE(L"Video renderer ADL - timing report failed: the reported vertical display frequency is invalid\n");
                        goto NoReportFound;
                    }

                    /* the Horizontal Frequency report often returns a bogus amount (some bytes 0, some filled in), unless the reliability can be improved, only use the standard report for now
                    // try to enhance precision with the help of the seperate Horizontal Frequency report (it has 2 to 4 valid bytes instead of just 2 bytes from the previous report)
                    // the Vertical Frequency report only has use for frequencies above 655.35 Hz (both the Vertical Frequency report and standard report should return this data in 0.01 Hz units)
                    // the Vertical Frequency report also didn't work on a few displays I tested; it returned nearly the same data as the Horizontal Frequency report
                    #ifdef _DEBUG
                    static wchar_t const szNotEnhancedPrec[] = L" not";
                    static wchar_t const szkEnhancedPrec[] = L"";
                    wchar_t const* szIsEnhancedPrec = szNotEnhancedPrec;
                    #endif
                    iOutputsize = 256;
                    static unsigned __int8 const aku8HorInput[] = {0x6E, 0x51, 0x82, 0x01, 0xAC, 0x6E ^ 0x51 ^ 0x82 ^ 0x01 ^ 0xAC};// command: "Get VCP Feature & VCP Feature Reply", VCP op code 0xAC: "Horizontal Frequency", checksum XOR at end
                    if (ADL_OK == AMD.m_fnADL_Display_DDCBlockAccess_Get(iAdapterIndex, iDisplayIndex, 0, 0, sizeof(aku8HorInput), aku8HorInput, &iOutputsize, au8Output)) {
                        // scan for the reply message
                        unsigned __int8* pOutI = au8Output;
                        unsigned __int8 k = static_cast<unsigned __int8>(iOutputsize - 10);// the following method needs the 11 bytes in the reply message (unsigned counter breaks at 0)
                        do {
                            unsigned __int32 u32rawb1 = *reinterpret_cast<unsigned __int32 __unaligned*>(pOutI);
                            if (u32rawb1 == 0x0002886E) {// 0x6E: Source address, 0x88: 0x80 + actual message size (without address bytes, this size byte and the checksum), 0x02: VCP Feature reply op code,  0x00: result code for NoError
                                if (pOutI[4] == 0xAC) {// 0xAC: VCP op code "Horizontal Frequency"
                                    // verify checksum
                                    // the message is 11 bytes in total, 6 bytes have already been read
                                    unsigned __int8 u8MH = pOutI[6], u8ML = pOutI[7], u8SH = pOutI[8], u8SL = pOutI[9];
                                    if (((u8MH != 0xFF) || (u8ML != 0xFF) || (u8SH != 0xFF) || (u8SL != 0xFF))// 0xFFFFFFFF signals invalid
                                        && (u8MH || u8ML || u8SH || u8SL)) {// 0 means invalid
                                        unsigned __int8 u8TP = pOutI[5], u8CHK50 = pOutI[10];// type code is ignored, except for the checksum
                                        unsigned __int8 u8CS = 0x50 ^ 0x6E ^ 0x88 ^ 0x02 ^ 0x00 ^ 0xAC ^ u8TP ^ u8MH ^ u8ML ^ u8SH ^ u8SL;// the checksum XOR at the end is computed using the 0x50 virtual host address
                                        if (u8CS == u8CHK50) {
                                            unsigned __int32 u32fused = static_cast<unsigned __int32>(u8ML) | static_cast<unsigned __int32>(u8SH) << 8 | static_cast<unsigned __int32>(u8SL) << 16;
                                            dwFh = u32fused + 100000 * static_cast<unsigned __int32>(u8MH);// byte MH is used as the + 100000 * [0x00, 0xFF] counter in some displays, the three other bytes are normal, big endian values
                                            DEBUG_ONLY(szIsEnhancedPrec = szkEnhancedPrec);
                                        }
                                    }
                                }
                                break;
                            }
                            ++pOutI;
                        } while (--k);
                    }
                    */
#ifdef _DEBUG
                    static wchar_t const szVHkStFactor[] = L"1";
                    static wchar_t const szVsFactor[] = L".001";
                    static wchar_t const szVmFactor[] = L".01";
                    static wchar_t const szVlFactor[] = L".1";
                    wchar_t const* szVerFactor = szVHkStFactor;
#endif
                    // the standard specifies 0.01 Hz units, but some manufacturers don't set this right
                    // 23 Hz is the lower than the lowest common vertical refresh rate of 24/1.001 Hz
                    // 230 Hz is a lot more than the common display connectors can carry with normal video resolutions
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
                    __m128d xFv  = _mm_setzero_pd();
                    xFv = _mm_cvtsi32_sd(xFv, dwFv);// 1 Hz units
                    if (dwFv >= 23000) {// 0.001 Hz units
                        DEBUG_ONLY(szVerFactor = szVsFactor);
                        static __declspec(align(16)) double const dkFvMs = 0.001;
                        xFv = _mm_mul_sd(xFv, *reinterpret_cast<__m128d const*>(&dkFvMs));
                    } else if (dwFv >= 2300) {// 0.01 Hz units
                        DEBUG_ONLY(szVerFactor = szVmFactor);
                        static __declspec(align(16)) double const dkFvMm = 0.01;
                        xFv = _mm_mul_sd(xFv, *reinterpret_cast<__m128d const*>(&dkFvMm));
                    } else if (dwFv >= 230) {// 0.1 Hz units
                        DEBUG_ONLY(szVerFactor = szVlFactor);
                        static __declspec(align(16)) double const dkFvMl = 0.1;
                        xFv = _mm_mul_sd(xFv, *reinterpret_cast<__m128d const*>(&dkFvMl));
                    }
#else
                    double dFv = static_cast<double>(static_cast<__int32>(dwFv));// 1 Hz units, the standard converter only does a proper job with signed values
                    if (dwFv >= 23000) {// 0.001 Hz units
                        DEBUG_ONLY(szVerFactor = szVsFactor);
                        dFv *= 0.001;
                    } else if (dwFv >= 2300) {// 0.01 Hz units
                        DEBUG_ONLY(szVerFactor = szVmFactor);
                        dFv *= 0.01;
                    } else if (dwFv >= 230) {// 0.1 Hz units
                        DEBUG_ONLY(szVerFactor = szVlFactor);
                        dFv *= 0.1;
                    }
#endif
#ifdef _DEBUG
                    static wchar_t const szHlFactor[] = L"1000";
                    static wchar_t const szHmFactor[] = L"100";
                    static wchar_t const szHsFactor[] = L"10";
                    wchar_t const* szHorFactor = szVHkStFactor;
#endif
                    // the standard specifies 1 Hz units, some manufacturers don't set this right (independent from the data for vertical refresh rate)
                    // 15 kHz is the old FM/NTSC audio/PAL&SECAM audio bandwidth (NTSC video is at 15750 Hz B/W or 15750/1.001 color, PAL&SECAM video is at 15625 Hz), it's rather unlikely that a manufacturer would use anything lower than that
                    // 150 kHz can be exceeded easily; We just assume that if the horizontal frequency is equal or larger than 15000, it is set correctly in 1 Hz units
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
                    __m128d xFh  = _mm_setzero_pd();
                    xFh = _mm_cvtsi32_sd(xFh, dwFh);// 1 Hz units
                    if (dwFh < 150) {// 1000 Hz units
                        DEBUG_ONLY(szHorFactor = szHlFactor);
                        static __declspec(align(16)) double const dkFhMl = 1000.0;
                        xFh = _mm_mul_sd(xFh, *reinterpret_cast<__m128d const*>(&dkFhMl));
                    } else if (dwFv < 1500) {// 100 Hz units
                        DEBUG_ONLY(szHorFactor = szHmFactor);
                        static __declspec(align(16)) double const dkFhMm = 100.0;
                        xFh = _mm_mul_sd(xFh, *reinterpret_cast<__m128d const*>(&dkFhMm));
                    } else if (dwFv < 15000) {// 10 Hz units
                        DEBUG_ONLY(szHorFactor = szHsFactor);
                        static __declspec(align(16)) double const dkFhMs = 10.0;
                        xFh = _mm_mul_sd(xFh, *reinterpret_cast<__m128d const*>(&dkFhMs));
                    }
#else
                    double dFh = static_cast<double>(static_cast<__int32>(dwFh));// 1 Hz units, the standard converter only does a proper job with signed values
                    if (dwFh < 150) {// 1000 Hz units
                        DEBUG_ONLY(szHorFactor = szHlFactor);
                        dFh *= 1000.0;
                    } else if (dwFh < 1500) {// 100 Hz units
                        DEBUG_ONLY(szHorFactor = szHmFactor);
                        dFh *= 100.0;
                    } else if (dwFh < 15000) {// 10 Hz units
                        DEBUG_ONLY(szHorFactor = szHsFactor);
                        dFh *= 10.0;
                    }
#endif
#if _M_IX86_FP != 1// SSE2 code, don't use on SSE builds, works correctly for x64
                    static __declspec(align(16)) __int64 const aki64LargeExp[2] = {0x4330000000000000, 0x4330000000000000};// 1<<52
                    __m128d xLargeExp = _mm_load_pd(reinterpret_cast<double const*>(aki64LargeExp));
                    __m128d xRate = xFh;
                    __m128d xFhCopy = xFh;
                    xFh = _mm_div_sd(xFh, xFv);// horizontal / vertical frequency yields the total scan line count, which we round, as this is always a whole amount

                    // quick rounding of a positive number that is much smaller than the maximum double size
                    ASSERT(_MM_GET_ROUNDING_MODE() == _MM_ROUND_NEAREST);
                    xFh = _mm_add_sd(xFh, xLargeExp);
                    xFh = _mm_sub_sd(xFh, xLargeExp);

                    // pack two vectors as {a, b} and {b, a} before the SIMD division
                    xRate = _mm_unpacklo_pd(xRate, xFh);
                    xFh = _mm_unpacklo_pd(xFh, xFhCopy);

                    xRate = _mm_div_pd(xRate, xFh);// this is generally more precise than just dFv, because of the scan line counting
                    _mm_store_sd(&m_dDetectedScanlinesPerFrame, xFh);
                    _mm_store_sd(&m_dReferenceRefreshRate, xRate);
                    _mm_store_pd(&m_dDetectedRefreshRate, xRate);// also stores m_dDetectedRefreshTime
                    DEBUG_ONLY(TRACE(L"Video renderer ADL - timing report succeeded: %f Hz, %u scan lines, raw vf %hu * %s, raw hf %hu * %s\n", _mm_cvtsd_f64(xRate), _mm_cvtsd_si32(xFh), dwFv, szVerFactor, dwFh, szHorFactor));
#else
                    double dTotalScanLineCount = floor(dFh / dFv + 0.5);// horizontal / vertical frequency yields the total scan line count, which we round, as this is always a whole amount
                    double dRefreshRate = dFh / dTotalScanLineCount;// this is generally more precise than just dFv, because of the scan line counting
                    m_dDetectedRefreshRate = m_dReferenceRefreshRate = dRefreshRate;
                    m_dDetectedScanlinesPerFrame = dTotalScanLineCount;
                    m_dDetectedRefreshTime = dTotalScanLineCount / dFh;
                    DEBUG_ONLY(TRACE(L"Video renderer ADL - timing report succeeded: %f Hz, %u scan lines, raw vf %hu * %s, raw hf %hu * %s\n", dRefreshRate, static_cast<__int32>(dTotalScanLineCount), dwFv, szVerFactor, dwFh, szHorFactor));
#endif
                    bReturnValue = true;
                }
NoReportFound:
                i = 0;
                break;
            } else {
                if (--u8RetryCount) {
                    TRACE(L"Video renderer ADL - timing report failed the &s try: no valid reply from the display device, the renderer is giving the driver half a second and will try again\n", (u8RetryCount == 2) ? L"first" : L"second");
                    Sleep(500);
                    goto RetryGetReport;
                }
                TRACE(L"Video renderer ADL - timing report failed the third try: no valid reply from the display device\n");
            }
        }
        free(pAdlDisplayInfo);
    } while (--i >= 0);

    if (!bReturnValue) {
        TRACE(L"Video renderer ADL - timing report failed, so reverted to the old method\n");
    }
    free(pAdapterInfo);
    AMD.m_fnADL_Main_Control_Destroy();

    EXECUTE_ASSERT(FreeLibrary(hADL));
    return bReturnValue;
}
