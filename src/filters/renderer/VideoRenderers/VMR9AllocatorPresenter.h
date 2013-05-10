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
#include "OuterVMR.h"
#include "IPinHook.h"
#include <moreuuids.h>

namespace DSObjects
{
    class CVMR9AllocatorPresenter
        : public CDX9AllocatorPresenter
        , public IVMRSurfaceAllocator9
        , public IVMRImagePresenter9
        , public IVMRWindowlessControl9
    {
        __declspec(nothrow noalias) __forceinline ~CVMR9AllocatorPresenter() { UnhookNewSegmentAndReceive(); }

    public:
        COuterVMR m_OuterVMR;// warning: m_OuterVMR does not hold a reference inside this class, it's given away in CFGFilterVideoRenderer::Create() and m_OuterVMR is in command of doing the last Release() of this class
        IVMRSurfaceAllocatorNotify9* m_pIVMRSurfAllocNotify;// destroyed by m_OuterVMR before this class
    private:
        HANDLE m_hEvtReset;
        unsigned __int8 m_u8InputVMR9MixerSurface;
        bool m_bUseInternalTimer;
        bool m_bNeedCheckSample;

    public:
        // IUnknown
        __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv);
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef();
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release();

        // CSubPicAllocatorPresenterImpl
        __declspec(nothrow noalias) void ResetDevice();

        // IVMRSurfaceAllocator9
        __declspec(nothrow noalias) STDMETHODIMP InitializeDevice(DWORD_PTR dwUserID, VMR9AllocationInfo* lpAllocInfo, DWORD* lpNumBuffers);
        __declspec(nothrow noalias) STDMETHODIMP TerminateDevice(DWORD_PTR dwID);
        __declspec(nothrow noalias) STDMETHODIMP GetSurface(DWORD_PTR dwUserID, DWORD SurfaceIndex, DWORD SurfaceFlags, IDirect3DSurface9** lplpSurface);
        __declspec(nothrow noalias) STDMETHODIMP AdviseNotify(IVMRSurfaceAllocatorNotify9* lpIVMRSurfAllocNotify);

        // IVMRImagePresenter9
        __declspec(nothrow noalias) STDMETHODIMP StartPresenting(DWORD_PTR dwUserID);
        __declspec(nothrow noalias) STDMETHODIMP StopPresenting(DWORD_PTR dwUserID);
        __declspec(nothrow noalias) STDMETHODIMP PresentImage(DWORD_PTR dwUserID, VMR9PresentationInfo* lpPresInfo);

        // IVMRWindowlessControl9
        __declspec(nothrow noalias) STDMETHODIMP GetNativeVideoSize(LONG* lpWidth, LONG* lpHeight, LONG* lpARWidth, LONG* lpARHeight);
        __declspec(nothrow noalias) STDMETHODIMP GetMinIdealVideoSize(LONG* lpWidth, LONG* lpHeight);
        __declspec(nothrow noalias) STDMETHODIMP GetMaxIdealVideoSize(LONG* lpWidth, LONG* lpHeight);
        __declspec(nothrow noalias) STDMETHODIMP SetVideoPosition(LPRECT const lpSRCRect, LPRECT const lpDSTRect);
        __declspec(nothrow noalias) STDMETHODIMP GetVideoPosition(LPRECT lpSRCRect, LPRECT lpDSTRect);
        __declspec(nothrow noalias) STDMETHODIMP GetAspectRatioMode(DWORD* lpAspectRatioMode);
        __declspec(nothrow noalias) STDMETHODIMP SetAspectRatioMode(DWORD AspectRatioMode);
        __declspec(nothrow noalias) STDMETHODIMP SetVideoClippingWindow(HWND hwnd);
        __declspec(nothrow noalias) STDMETHODIMP RepaintVideo(HWND hwnd, HDC hdc);
        __declspec(nothrow noalias) STDMETHODIMP DisplayModeChanged();
        __declspec(nothrow noalias) STDMETHODIMP GetCurrentImage(BYTE** lpDib);
        __declspec(nothrow noalias) STDMETHODIMP SetBorderColor(COLORREF Clr);
        __declspec(nothrow noalias) STDMETHODIMP GetBorderColor(COLORREF* lpClr);

        __declspec(nothrow noalias) __forceinline CVMR9AllocatorPresenter(__in HWND hWnd, __inout CStringW* pstrError)
            : CDX9AllocatorPresenter(hWnd, pstrError, false)
            , m_pIVMRSurfAllocNotify(nullptr)
            , m_hEvtReset(nullptr)
            , m_bUseInternalTimer(false)
            , m_bNeedCheckSample(true)
            , m_u8InputVMR9MixerSurface(0) {
            ASSERT(pstrError);

            if (!pstrError->IsEmpty()) {// CDX9AllocatorPresenter() failed
                return;
            }

            HRESULT hr;
            // create the outer VMR
            hr = CoCreateInstance(CLSID_VideoMixingRenderer9, static_cast<IUnknown*>(static_cast<IBaseFilter*>(&m_OuterVMR)), CLSCTX_ALL, IID_IUnknown, reinterpret_cast<void**>(&m_OuterVMR.m_pVMR));// IBaseFilter is at Vtable location 0
            if (FAILED(hr)) {
                *pstrError = L"CoCreateInstance() of VMR-9 failed\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                ASSERT(0);
                return;
            }

            // note: m_OuterVMR gets a reference count of 1, because of the following:
            hr = m_OuterVMR.m_pVMR->QueryInterface(IID_IBaseFilter, reinterpret_cast<void**>(&m_OuterVMR.m_pBaseFilter));// m_pBaseFilter does not hold a reference inside COuterVMR
            ASSERT(hr == S_OK);// interface is known to be part of VMR-9
            // on all return because of failure cases of this function use: m_OuterVMR.m_pVMR->Release(); do not let the reference count of m_OuterVMR hit 0 because of self-destruction issues

            IPin* pPin = GetFirstPin(m_OuterVMR.m_pBaseFilter);// GetFirstPin returns a pointer without incrementing the reference count
            void* pVoid;
            if (SUCCEEDED(pPin->QueryInterface(IID_IMemInputPin, &pVoid))) {
                // depending on situation, may be skipped
                // No NewSegment : no chocolate :o)
                m_bUseInternalTimer = HookNewSegmentAndReceive(pPin, reinterpret_cast<IMemInputPin*>(pVoid));
                reinterpret_cast<IMemInputPin*>(pVoid)->Release();
            }

            if (SUCCEEDED(pPin->QueryInterface(IID_IAMVideoAccelerator, &pVoid))) {
                // depending on situation, may be skipped
                HookAMVideoAccelerator(reinterpret_cast<IAMVideoAccelerator*>(pVoid));
                reinterpret_cast<IAMVideoAccelerator*>(pVoid)->Release();
            }

            hr = m_OuterVMR.m_pVMR->QueryInterface(IID_IVMRFilterConfig9, &pVoid);
            ASSERT(hr == S_OK);// interface is known to be part of VMR-9

            hr = reinterpret_cast<IVMRFilterConfig9*>(pVoid)->SetRenderingMode(VMR9Mode_Renderless);
            if (FAILED(hr)) {
                reinterpret_cast<IVMRFilterConfig9*>(pVoid)->Release();
                *pstrError = L"IVMRFilterConfig9::SetRenderingMode() failed\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                ASSERT(0);
                m_OuterVMR.m_pVMR->Release();
                return;
            }
            hr = reinterpret_cast<IVMRFilterConfig9*>(pVoid)->SetNumberOfStreams(1);
            reinterpret_cast<IVMRFilterConfig9*>(pVoid)->Release();
            if (FAILED(hr)) {
                *pstrError = L"IVMRFilterConfig9::SetNumberOfStreams() failed\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                ASSERT(0);
                m_OuterVMR.m_pVMR->Release();
                return;
            }

            hr = m_OuterVMR.m_pVMR->QueryInterface(IID_IVMRMixerControl9, &pVoid);
            ASSERT(hr == S_OK);// interface is known to be part of VMR-9

            DWORD dwPrefs;
            hr = reinterpret_cast<IVMRMixerControl9*>(pVoid)->GetMixingPrefs(&dwPrefs);
            if (FAILED(hr)) {
                reinterpret_cast<IVMRMixerControl9*>(pVoid)->Release();
                *pstrError = L"IVMRMixerControl9::GetMixingPrefs() failed\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                ASSERT(0);
                m_OuterVMR.m_pVMR->Release();
                return;
            }
            // See http://msdn.microsoft.com/en-us/library/dd390928(VS.85).aspx , YUV mixer mode won't work with Vista or newer
            dwPrefs |= MixerPref9_NonSquareMixing | MixerPref9_NoDecimation;
            if ((m_u8OSVersionMajor < 6) && mk_pRendererSettings->fVMR9MixerYUV) {
                dwPrefs = dwPrefs & ~MixerPref9_RenderTargetMask | MixerPref9_RenderTargetYUV;
            }
            hr = reinterpret_cast<IVMRMixerControl9*>(pVoid)->SetMixingPrefs(dwPrefs);
            reinterpret_cast<IVMRMixerControl9*>(pVoid)->Release();
            if (FAILED(hr)) {
                *pstrError = L"IVMRMixerControl9::SetMixingPrefs() failed\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                ASSERT(0);
                m_OuterVMR.m_pVMR->Release();
                return;
            }

            hr = m_OuterVMR.m_pVMR->QueryInterface(IID_IVMRSurfaceAllocatorNotify9, &pVoid);
            ASSERT(hr == S_OK);// interface is known to be part of VMR-9

            __assume(this);// fix assembly: the compiler generated tests for null pointer input on static_cast<T>(this)
            hr = reinterpret_cast<IVMRSurfaceAllocatorNotify9*>(pVoid)->AdviseSurfaceAllocator(0x6ABE51, static_cast<IVMRSurfaceAllocator9*>(this));
            if (FAILED(hr)) {
                reinterpret_cast<IVMRSurfaceAllocatorNotify9*>(pVoid)->Release();
                *pstrError = L"IVMRSurfaceAllocatorNotify9::AdviseSurfaceAllocator() failed\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                ASSERT(0);
                m_OuterVMR.m_pVMR->Release();
                return;
            }
            hr = AdviseNotify(reinterpret_cast<IVMRSurfaceAllocatorNotify9*>(pVoid));// reference inherited
            if (FAILED(hr)) {
                *pstrError = L"IVMRSurfaceAllocatorNotify9::SetD3DDevice() failed\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                ASSERT(0);
                m_OuterVMR.m_pVMR->Release();
                return;
            }
        }
    };
}
