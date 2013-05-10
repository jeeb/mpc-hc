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
#include "VMR9AllocatorPresenter.h"
#include <intrin.h>

using namespace DSObjects;

// IUnknown

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::QueryInterface(REFIID riid, __deref_out void** ppv)
{
    ASSERT(ppv);
    __assume(this);// fix assembly: the compiler generated tests for null pointer input on static_cast<T>(this)

    __int64 lo = reinterpret_cast<__int64 const*>(&riid)[0], hi = reinterpret_cast<__int64 const*>(&riid)[1];
    if (lo == reinterpret_cast<__int64 const*>(&IID_IUnknown)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IUnknown)[1]) {// CVMR9AllocatorPresenter may not take IUnknown in this case
        *ppv = static_cast<IUnknown*>(static_cast<IBaseFilter*>(this));// IBaseFilter is at Vtable location 0
        ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
        ASSERT(ulRef);
        UNREFERENCED_PARAMETER(ulRef);
        return NOERROR;
    }
    CVMR9AllocatorPresenter* pAllocatorPresenter = reinterpret_cast<CVMR9AllocatorPresenter*>(reinterpret_cast<uintptr_t>(this) - offsetof(CVMR9AllocatorPresenter, m_OuterVMR));// inverse relationship with CVMR9AllocatorPresenter
    return pAllocatorPresenter->QueryInterface(riid, ppv);// CVMR9AllocatorPresenter has the complete list of interfaces for this renderer
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) COuterVMR::AddRef()
{
    // based on CUnknown::NonDelegatingAddRef()
    // the original CUnknown::NonDelegatingAddRef() has a version that keeps compatibility for Windows 95, Windows NT 3.51 and earlier, this one doesn't
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    return ulRef;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) COuterVMR::Release()
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

        CVMR9AllocatorPresenter* pAllocatorPresenter = reinterpret_cast<CVMR9AllocatorPresenter*>(reinterpret_cast<uintptr_t>(this) - offsetof(CVMR9AllocatorPresenter, m_OuterVMR));// inverse relationship with CVMR9AllocatorPresenter
        pAllocatorPresenter->m_pIVMRSurfAllocNotify->Release();// object depends on the external VMR-9
        m_pVMR->Release();
        pAllocatorPresenter = reinterpret_cast<CVMR9AllocatorPresenter*>(reinterpret_cast<uintptr_t>(this) - offsetof(CVMR9AllocatorPresenter, m_OuterVMR));// inverse relationship with CVMR9AllocatorPresenter
        pAllocatorPresenter->Release();// causes self-destruction
        return 0;
    } else {
        // Don't touch the counter again even in this leg as the object
        // may have just been released on another thread too
        return ulRef;
    }
}

// IBaseFilter

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::EnumPins(__out IEnumPins** ppEnum)
{
    return m_pBaseFilter->EnumPins(ppEnum);
}

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::FindPin(LPCWSTR Id, __out IPin** ppPin)
{
    return m_pBaseFilter->FindPin(Id, ppPin);
}

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::QueryFilterInfo(__out FILTER_INFO* pInfo)
{
    return m_pBaseFilter->QueryFilterInfo(pInfo);
}

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::JoinFilterGraph(__in_opt IFilterGraph* pGraph, __in_opt LPCWSTR pName)
{
    return m_pBaseFilter->JoinFilterGraph(pGraph, pName);
}

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::QueryVendorInfo(__out LPWSTR* pVendorInfo)
{
    return m_pBaseFilter->QueryVendorInfo(pVendorInfo);
}

// IMediaFilter

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::Stop()
{
    // return m_pBaseFilter->Stop(); this function still deadlocks on closing, the VMR also doesn't refresh its window ever in stopped mode, which leaves the artifacts of the previous frames
    return S_OK;
}

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::Pause()
{
    return m_pBaseFilter->Pause();
}

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::Run(REFERENCE_TIME tStart)
{
    return m_pBaseFilter->Run(tStart);
}

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::GetState(DWORD dwMilliSecsTimeout, __out FILTER_STATE* State)
{
    return m_pBaseFilter->GetState(dwMilliSecsTimeout, State);
}

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::SetSyncSource(__in_opt IReferenceClock* pClock)
{
    return m_pBaseFilter->SetSyncSource(pClock);
}

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::GetSyncSource(__deref_out_opt IReferenceClock** pClock)
{
    return m_pBaseFilter->GetSyncSource(pClock);
}

// IPersist

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::GetClassID(__RPC__out CLSID* pClassID)
{
    return m_pBaseFilter->GetClassID(pClassID);
}

// IVideoWindow

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::get_Width(long* pWidth)
{
    ASSERT(pWidth);

    IVMRWindowlessControl9* pWC9;
    if (SUCCEEDED(m_pVMR->QueryInterface(IID_IVMRWindowlessControl9, reinterpret_cast<void**>(&pWC9)))) {
        RECT s, d;
        HRESULT hr = pWC9->GetVideoPosition(&s, &d);
        *pWidth = s.right - s.left;
        return hr;
    }
    return E_NOTIMPL;
}

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::get_Height(long* pHeight)
{
    ASSERT(pHeight);

    IVMRWindowlessControl9* pWC9;
    if (SUCCEEDED(m_pVMR->QueryInterface(IID_IVMRWindowlessControl9, reinterpret_cast<void**>(&pWC9)))) {
        RECT s, d;
        HRESULT hr = pWC9->GetVideoPosition(&s, &d);
        *pHeight = s.bottom - s.top;
        return hr;
    }
    return E_NOTIMPL;
}

// IBasicVideo2
__declspec(nothrow noalias) STDMETHODIMP COuterVMR::GetSourcePosition(long* pLeft, long* pTop, long* pWidth, long* pHeight)
{
    ASSERT(pLeft);
    ASSERT(pTop);
    ASSERT(pWidth);
    ASSERT(pHeight);

    // DVD Nav. bug workaround fix
    *pLeft = *pTop = 0;
    return GetVideoSize(pWidth, pHeight);

    /*
    IVMRWindowlessControl9* pWC9;
    if (SUCCEEDED(m_pVMR->QueryInterface(IID_IVMRWindowlessControl9, reinterpret_cast<void**>(&pWC9)))) {
        RECT s, d;
        HRESULT hr = pWC9->GetVideoPosition(&s, &d);
        *pLeft = s.left;
        *pTop = s.top;
        *pWidth = s.right - s.left;
        *pHeight = s.bottom - s.top;
        return hr;
    }
    return E_NOTIMPL;
    */
}

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::GetDestinationPosition(long* pLeft, long* pTop, long* pWidth, long* pHeight)
{
    ASSERT(pLeft);
    ASSERT(pTop);
    ASSERT(pWidth);
    ASSERT(pHeight);

    IVMRWindowlessControl9* pWC9;
    if (SUCCEEDED(m_pVMR->QueryInterface(IID_IVMRWindowlessControl9, reinterpret_cast<void**>(&pWC9)))) {
        RECT s, d;
        HRESULT hr = pWC9->GetVideoPosition(&s, &d);
        *pLeft = d.left;
        *pTop = d.top;
        *pWidth = d.right - d.left;
        *pHeight = d.bottom - d.top;
        return hr;
    }
    return E_NOTIMPL;
}

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::GetVideoSize(long* pWidth, long* pHeight)
{
    ASSERT(pWidth);
    ASSERT(pHeight);

    IVMRWindowlessControl9* pWC9;
    if (SUCCEEDED(m_pVMR->QueryInterface(IID_IVMRWindowlessControl9, reinterpret_cast<void**>(&pWC9)))) {
        LONG aw, ah;
        //return pWC9->GetNativeVideoSize(pWidth, pHeight, &aw, &ah);
        // DVD Nav. bug workaround fix
        HRESULT hr = pWC9->GetNativeVideoSize(pWidth, pHeight, &aw, &ah);
        *pWidth = *pHeight * aw / ah;
        return hr;
    }
    return E_NOTIMPL;
}

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::GetPreferredAspectRatio(long* plAspectX, long* plAspectY)
{
    IVMRWindowlessControl9* pWC9;
    if (SUCCEEDED(m_pVMR->QueryInterface(IID_IVMRWindowlessControl9, reinterpret_cast<void**>(&pWC9)))) {
        LONG w, h;
        return pWC9->GetNativeVideoSize(&w, &h, plAspectX, plAspectY);
    }
    return E_NOTIMPL;
}

// IKsPropertySet - MacrovisionKicker

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::Set(REFGUID PropSet, ULONG Id, LPVOID pInstanceData, ULONG InstanceLength, LPVOID pPropertyData, ULONG DataLength)
{
    IKsPropertySet* pKsPS;
    if (SUCCEEDED(m_pVMR->QueryInterface(IID_IKsPropertySet, reinterpret_cast<void**>(&pKsPS)))) {
        HRESULT hr;
        if (PropSet == AM_KSPROPSETID_CopyProt && Id == AM_PROPERTY_COPY_MACROVISION) {
            TRACE(L"Oops, no-no-no, no macrovision please\n");
            hr = S_OK;
        } else {
            hr = pKsPS->Set(PropSet, Id, pInstanceData, InstanceLength, pPropertyData, DataLength);
        }
        pKsPS->Release();
        return hr;
    }
    return E_UNEXPECTED;
}

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::Get(REFGUID PropSet, ULONG Id, LPVOID pInstanceData, ULONG InstanceLength, LPVOID pPropertyData, ULONG DataLength, ULONG* pBytesReturned)
{
    IKsPropertySet* pKsPS;
    if (SUCCEEDED(m_pVMR->QueryInterface(IID_IKsPropertySet, reinterpret_cast<void**>(&pKsPS)))) {
        HRESULT hr = pKsPS->Get(PropSet, Id, pInstanceData, InstanceLength, pPropertyData, DataLength, pBytesReturned);
        pKsPS->Release();
        return hr;
    }
    return E_UNEXPECTED;
}

__declspec(nothrow noalias) STDMETHODIMP COuterVMR::QuerySupported(REFGUID PropSet, ULONG Id, ULONG* pTypeSupport)
{
    IKsPropertySet* pKsPS;
    if (SUCCEEDED(m_pVMR->QueryInterface(IID_IKsPropertySet, reinterpret_cast<void**>(&pKsPS)))) {
        HRESULT hr = pKsPS->QuerySupported(PropSet, Id, pTypeSupport);
        pKsPS->Release();
        return hr;
    }
    return E_UNEXPECTED;
}
