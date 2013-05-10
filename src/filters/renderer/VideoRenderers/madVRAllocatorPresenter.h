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

#include "AllocatorCommon.h"
#include "../../../SubPic/DX9SubPic.h"
#include "../../../SubPic/ISubRender.h"
#include "moreuuids.h"

interface __declspec(uuid("ABA34FDA-DD22-4E00-9AB4-4ABF927D0B0C") novtable)
IMadVRTextOsd :
public IUnknown {
    STDMETHOD(OsdDisplayMessage)(LPCWSTR text, DWORD milliseconds) = 0;
    STDMETHOD(OsdClearMessage)() = 0;
};

interface IMadVRExternalPixelShaders;

namespace DSObjects
{
    class CmadVRAllocatorPresenter
        : public CSubPicAllocatorPresenterImpl
    {
        class CSubRenderCallback
            : public ISubRenderCallback2
        {
            __declspec(nothrow noalias) __forceinline CSubRenderCallback() {}

            CmadVRAllocatorPresenter* m_pDXRAP;// doesn't hold a reference
            CCritSec m_csLock;
            ULONG volatile mv_ulReferenceCount;

        public:
            __declspec(nothrow noalias) __forceinline CSubRenderCallback(CmadVRAllocatorPresenter* pDXRAP)
                : m_pDXRAP(pDXRAP)
                , mv_ulReferenceCount(1) {}

            // IUnknown
            __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv) {
                ASSERT(ppv);
                __assume(this);// fix assembly: the compiler generated tests for null pointer input on static_cast<T>(this)

                __int64 lo = reinterpret_cast<__int64 const*>(&riid)[0], hi = reinterpret_cast<__int64 const*>(&riid)[1];
                void* pv = static_cast<IUnknown*>(this);
                if (lo == reinterpret_cast<__int64 const*>(&IID_IUnknown)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IUnknown)[1]) {
                    goto exit;
                }
                pv = static_cast<ISubRenderCallback2*>(this);
                if (lo == reinterpret_cast<__int64 const*>(&__uuidof(ISubRenderCallback2))[0] && hi == reinterpret_cast<__int64 const*>(&__uuidof(ISubRenderCallback2))[1]) {
                    goto exit;
                }
                pv = static_cast<ISubRenderCallback*>(this);
                if (lo == reinterpret_cast<__int64 const*>(&__uuidof(ISubRenderCallback))[0] && hi == reinterpret_cast<__int64 const*>(&__uuidof(ISubRenderCallback))[1]) {
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
            __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef() {// based on CUnknown::NonDelegatingAddRef()
                // the original CUnknown::NonDelegatingAddRef() has a version that keeps compatibility for Windows 95, Windows NT 3.51 and earlier, this one doesn't
                ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
                ASSERT(ulRef);
                return ulRef;
            }
            __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release() {// based on CUnknown::NonDelegatingRelease()
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
                    this->~CSubRenderCallback();
                    free(this);
                    return 0;
                } else {
                    // Don't touch the counter again even in this leg as the object
                    // may have just been released on another thread too
                    return ulRef;
                }
            }

            __declspec(nothrow noalias) void __forceinline SetDXRAP(CmadVRAllocatorPresenter* pDXRAP) {
                CAutoLock cAutoLock(&m_csLock);
                m_pDXRAP = pDXRAP;
            }

            // ISubRenderCallback
            __declspec(nothrow noalias) STDMETHODIMP SetDevice(IDirect3DDevice9* pD3DDev) {
                CAutoLock cAutoLock(&m_csLock);
                if (m_pDXRAP) {
                    return m_pDXRAP->SetDevice(pD3DDev);
                }
                return E_UNEXPECTED;
            }

            __declspec(nothrow noalias) STDMETHODIMP Render(REFERENCE_TIME rtStart, int left, int top, int right, int bottom, int width, int height) {
                CAutoLock cAutoLock(&m_csLock);
                if (m_pDXRAP) {
                    return m_pDXRAP->Render(rtStart, 0, 0, left, top, right, bottom, width, height);
                }
                return E_UNEXPECTED;
            }

            // ISubRendererCallback2
            __declspec(nothrow noalias) STDMETHODIMP RenderEx(REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, REFERENCE_TIME AvgTimePerFrame, int left, int top, int right, int bottom, int width, int height) {
                CAutoLock cAutoLock(&m_csLock);
                if (m_pDXRAP) {
                    return m_pDXRAP->Render(rtStart, rtStop, AvgTimePerFrame, left, top, right, bottom, width, height);
                }
                return E_UNEXPECTED;
            }
        };

        __declspec(nothrow noalias) __forceinline ~CmadVRAllocatorPresenter() {
            if (m_pSRCB) {
                // nasty, but we have to let it know about our death somehow
                m_pSRCB->SetDXRAP(nullptr);
                m_pSRCB->Release();
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

            if (m_pDXR) {
                m_pDXR->Release();
            }
        }

        IUnknown* m_pDXR;
        CSubRenderCallback* m_pSRCB;

    public:
        // IUnknown
        __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv);
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef();
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release();

        // utility functions for CSubRenderCallback
        __declspec(nothrow noalias) HRESULT SetDevice(IDirect3DDevice9* pD3DDev);
        __declspec(nothrow noalias) HRESULT Render(
            REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, REFERENCE_TIME atpf,
            int left, int top, int bottom, int right, int width, int height);

        // CSubPicAllocatorPresenterImpl
        __declspec(nothrow noalias) void SetPosition(__in_ecount(2) RECT const arcWV[2]);
        __declspec(nothrow noalias) SIZE GetVideoSize(__in bool fCorrectAR) const;
        __declspec(nothrow noalias) HRESULT GetDIB(__out_opt void* pDib, __inout size_t* pSize);
        __declspec(nothrow noalias) uintptr_t SetPixelShaders(__in_ecount(2) CAtlList<Shader const*> const aList[2]);
        __declspec(nothrow noalias) void ClearPixelShaders(unsigned __int8 u8RenderStages);

        __declspec(nothrow noalias) __forceinline CmadVRAllocatorPresenter(__in HWND hWnd, __inout CStringW* pstrError, __out_opt IBaseFilter** ppRenderer)
            : CSubPicAllocatorPresenterImpl(hWnd)
            , m_pDXR(nullptr)
            , m_pSRCB(nullptr) {
            ASSERT(ppRenderer);

            HRESULT hr;
            __assume(this);// fix assembly: the compiler generated tests for null pointer input on static_cast<T>(this)
            if (FAILED(hr = CoCreateInstance(CLSID_madVR, static_cast<IUnknown*>(static_cast<CSubPicAllocatorPresenterImpl*>(this)), CLSCTX_ALL, IID_IUnknown, reinterpret_cast<void**>(&m_pDXR)))) {// CSubPicAllocatorPresenterImpl is at Vtable location 0
                ASSERT(0);
                *pstrError = L"CoCreateInstance() of madVR failed\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                return;
            }

            ISubRender* pSR;
            if (FAILED(hr = m_pDXR->QueryInterface(__uuidof(ISubRender), reinterpret_cast<void**>(&pSR)))) {
                ASSERT(0);
                m_pDXR->Release();
                m_pDXR = nullptr;
                *pstrError = L"failed interface query for ISubRender to madVR\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                return;
            }

            void* pRawMem = malloc(sizeof(CSubRenderCallback));
            if (!pRawMem) {
                ASSERT(0);
                pSR->Release();
                m_pDXR->Release();
                m_pDXR = nullptr;
                *pstrError = L"Out of memory error for creating CSubRenderCallback for madVR\n";
                return;
            }
            CSubRenderCallback* pSRCB = new(pRawMem) CSubRenderCallback(this);
            m_pSRCB = pSRCB;// reference inherited

            hr = pSR->SetCallback(pSRCB);
            pSR->Release();
            if (FAILED(hr)) {
                ASSERT(0);
                m_pDXR->Release();
                m_pDXR = nullptr;
                *pstrError = L"failed to set ISubRenderCallback on madVR\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
                return;
            }

            if (FAILED(hr = m_pDXR->QueryInterface(IID_IBaseFilter, reinterpret_cast<void**>(ppRenderer)))) {
                ASSERT(0);
                m_pDXR->Release();
                m_pDXR = nullptr;
                *pstrError = L"failed interface query for IBaseFilter to madVR\n";
                *pstrError += GetWindowsErrorMessage(hr, nullptr);
            }
        }
    };
}
