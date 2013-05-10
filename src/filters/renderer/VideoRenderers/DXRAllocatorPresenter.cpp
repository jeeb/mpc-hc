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
#include "RenderersSettings.h"
#include "DXRAllocatorPresenter.h"
#include <intrin.h>

using namespace DSObjects;

//
// CDXRAllocatorPresenter
//
// IUnknown

__declspec(nothrow noalias) STDMETHODIMP CDXRAllocatorPresenter::QueryInterface(REFIID riid, __deref_out void** ppv)
{
    ASSERT(ppv);
    __assume(this);// fix assembly: the compiler generated tests for null pointer input on static_cast<T>(this)

    __int64 lo = reinterpret_cast<__int64 const*>(&riid)[0], hi = reinterpret_cast<__int64 const*>(&riid)[1];
    void* pv = static_cast<IUnknown*>(static_cast<CSubPicAllocatorPresenterImpl*>(this));// CSubPicAllocatorPresenterImpl is at Vtable location 0
    if (lo == reinterpret_cast<__int64 const*>(&IID_IUnknown)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IUnknown)[1]) {
        goto exit;
    }
    pv = static_cast<CSubPicAllocatorPresenterImpl*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&__uuidof(CSubPicAllocatorPresenterImpl))[0] && hi == reinterpret_cast<__int64 const*>(&__uuidof(CSubPicAllocatorPresenterImpl))[1]) {
        goto exit;
    }
    // lastly, try the external renderer
    if (!m_pDXR) {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    return m_pDXR->QueryInterface(riid, ppv);
exit:
    *ppv = pv;
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    UNREFERENCED_PARAMETER(ulRef);
    return NOERROR;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CDXRAllocatorPresenter::AddRef()
{
    // based on CUnknown::NonDelegatingAddRef()
    // the original CUnknown::NonDelegatingAddRef() has a version that keeps compatibility for Windows 95, Windows NT 3.51 and earlier, this one doesn't
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    return ulRef;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CDXRAllocatorPresenter::Release()
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
        this->~CDXRAllocatorPresenter();
        free(this);
        return 0;
    } else {
        // Don't touch the counter again even in this leg as the object
        // may have just been released on another thread too
        return ulRef;
    }
}

__declspec(nothrow noalias) HRESULT CDXRAllocatorPresenter::SetDevice(IDirect3DDevice9* pD3DDev)
{
    // release subtitle resources
    if (m_pSubPicQueue) {
        m_pSubPicQueue->Release();
        m_pSubPicQueue = nullptr;
    }
    if (m_pSubPicAllocator) {
        m_pSubPicAllocator->Release();
        m_pSubPicAllocator = nullptr;
    }

    if (!pD3DDev) {
        return S_OK;
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
    if (mk_pRendererSettings->nSPCMaxRes) { // half and tree-quarters resolution
        i32Oleft >>= 1;
        i32Otop >>= 1;
        u32sx >>= 1;
        u32sy >>= 1;
        if (mk_pRendererSettings->nSPCMaxRes == 1) { // tree-quarters resolution
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
        return E_OUTOFMEMORY;
    }
    CDX9SubPicAllocator* pSubPicAllocator = new(pRawMem) CDX9SubPicAllocator(u32sx, u32sy, pD3DDev);
    m_pSubPicAllocator = static_cast<CSubPicAllocatorImpl*>(pSubPicAllocator);// reference inherited

    CSubPicQueueImpl* pSubPicQueue;
    if (mk_pRendererSettings->nSPCSize) {
        pRawMem = malloc(sizeof(CSubPicQueue));
        if (!pRawMem) {
            return E_OUTOFMEMORY;
        }
        pSubPicQueue = static_cast<CSubPicQueueImpl*>(new(pRawMem) CSubPicQueue(m_pSubPicAllocator, m_dDetectedVideoFrameRate, mk_pRendererSettings->nSPCSize, !mk_pRendererSettings->fSPCAllowAnimationWhenBuffering));
    } else {
        pRawMem = malloc(sizeof(CSubPicQueueNoThread));
        if (!pRawMem) {
            return E_OUTOFMEMORY;
        }
        pSubPicQueue = static_cast<CSubPicQueueImpl*>(new(pRawMem) CSubPicQueueNoThread(m_pSubPicAllocator, m_dDetectedVideoFrameRate));
    }
    m_pSubPicQueue = pSubPicQueue;// reference inherited
    if (m_pSubPicProvider) {
        pSubPicQueue->SetSubPicProvider(m_pSubPicProvider);
    }

    return S_OK;
}

__declspec(nothrow noalias) HRESULT CDXRAllocatorPresenter::Render(
    REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, REFERENCE_TIME atpf,
    int left, int top, int right, int bottom, int width, int height)
{
    {
        RECT arcWV[2] = {{0, 0, width, height}, {left, top, right, bottom}};
        __super::SetPosition(arcWV); // needed? should be already set by the player
    }
    SetTime(rtStart);
    if (atpf > 0) {
        m_dDetectedVideoFrameRate = RoundCommonRates(10000000.0 / static_cast<double>(atpf));
        m_pSubPicQueue->SetFPS(m_dDetectedVideoFrameRate);
    }

    // subtitles
    HRESULT hr = S_OK;
    if (CBSubPic* pSubPic = m_pSubPicQueue->LookupSubPic(m_i64Now)) {
        RECT arcSourceDest[2];
        if (pSubPic->GetSourceAndDest(arcSourceDest)) {
            CDX9SubPic* pDX9SubPic = static_cast<CDX9SubPic*>(pSubPic);
            IDirect3DDevice9* pD3DDev = pDX9SubPic->m_pD3DDev;

            if (FAILED(hr = pD3DDev->SetPixelShader(nullptr))
                    || FAILED(hr = pD3DDev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1))
                    || FAILED(hr = pD3DDev->SetTexture(0, pDX9SubPic->m_pTexture))
                    || FAILED(hr = pD3DDev->BeginScene())) {
                pSubPic->Release();
                return hr;
            }

            bool resized = (arcSourceDest->right - arcSourceDest->left != arcSourceDest[1].right - arcSourceDest[1].left) || (arcSourceDest->bottom - arcSourceDest->top != arcSourceDest[1].bottom - arcSourceDest[1].top);
            if (resized) {
                pD3DDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                pD3DDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            } else {
                pD3DDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
                pD3DDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            }

            // GetRenderState fails for devices created with D3DCREATE_PUREDEVICE
            // so we need to provide default values in case GetRenderState fails
            DWORD abe, sb, db;
            if (FAILED(pD3DDev->GetRenderState(D3DRS_ALPHABLENDENABLE, &abe))) {
                abe = FALSE;
            }
            if (FAILED(pD3DDev->GetRenderState(D3DRS_SRCBLEND, &sb))) {
                sb = D3DBLEND_ONE;
            }
            if (FAILED(pD3DDev->GetRenderState(D3DRS_DESTBLEND, &db))) {
                db = D3DBLEND_ZERO;
            }

            pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            pD3DDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);// pre-multiplied src
            pD3DDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCALPHA);// inverse alpha channel for dst
            pD3DDev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            pD3DDev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            pD3DDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            pD3DDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

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

            hr = pD3DDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(CUSTOMVERTEX_TEX1));
            pD3DDev->EndScene();

            pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, abe);
            pD3DDev->SetRenderState(D3DRS_SRCBLEND, sb);
            pD3DDev->SetRenderState(D3DRS_DESTBLEND, db);

            if (resized) {
                pD3DDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
                pD3DDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            }
        }
        pSubPic->Release();
    }

    return hr;
}

// CSubPicAllocatorPresenterImpl

__declspec(nothrow noalias) void CDXRAllocatorPresenter::SetPosition(__in_ecount(2) RECT const arcWV[2])
{
    ASSERT(arcWV);

    void* pVoid;
    if (SUCCEEDED(m_pDXR->QueryInterface(IID_IBasicVideo, &pVoid))) {
        IBasicVideo* pBV = reinterpret_cast<IBasicVideo*>(pVoid);
        pBV->SetDefaultSourcePosition();
        pBV->SetDestinationPosition(arcWV[1].left, arcWV[1].top, arcWV[1].right - arcWV[1].left, arcWV[1].bottom - arcWV[1].top);
        pBV->Release();
    }
    if (SUCCEEDED(m_pDXR->QueryInterface(IID_IVideoWindow, &pVoid))) {
        IVideoWindow* pVW = reinterpret_cast<IVideoWindow*>(pVoid);
        pVW->SetWindowPosition(arcWV[0].left, arcWV[0].top, arcWV[0].right - arcWV[0].left, arcWV[0].bottom - arcWV[0].top);
        pVW->Release();
    }
}

__declspec(nothrow noalias) SIZE CDXRAllocatorPresenter::GetVideoSize(__in bool fCorrectAR) const
{
    SIZE size = {0, 0};
    IBasicVideo2* pBV2;
    if (SUCCEEDED(m_pDXR->QueryInterface(IID_IBasicVideo2, reinterpret_cast<void**>(&pBV2)))) {
        if (!fCorrectAR) {
            pBV2->GetVideoSize(&size.cx, &size.cy);
        } else {
            pBV2->GetPreferredAspectRatio(&size.cx, &size.cy);
        }
        pBV2->Release();
    }
    return size;
}

__declspec(nothrow noalias) HRESULT CDXRAllocatorPresenter::GetDIB(__out_opt void* pDib, __inout size_t* pSize)
{
    HRESULT hr;
    IBasicVideo* pBV;
    if (SUCCEEDED(hr = m_pDXR->QueryInterface(IID_IBasicVideo, reinterpret_cast<void**>(&pBV)))) {
        hr = pBV->GetCurrentImage(reinterpret_cast<long*>(pSize), reinterpret_cast<long*>(pDib));
        pBV->Release();
    }
    return hr;
}
