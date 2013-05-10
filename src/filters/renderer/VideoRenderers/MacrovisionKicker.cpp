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
#include "MacrovisionKicker.h"

//
// CMacrovisionKicker
//

// IUnknown

__declspec(nothrow noalias) STDMETHODIMP CMacrovisionKicker::QueryInterface(REFIID riid, __deref_out void** ppv)
{
    ASSERT(ppv);
    __assume(this);// fix assembly: the compiler generated tests for null pointer input on static_cast<T>(this)

    __int64 lo = reinterpret_cast<__int64 const*>(&riid)[0], hi = reinterpret_cast<__int64 const*>(&riid)[1];
    void* pv = static_cast<IUnknown*>(this);// m_pInner may not take IUnknown
    if (lo == reinterpret_cast<__int64 const*>(&IID_IUnknown)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IUnknown)[1]) {
        goto exit;
    }
    pv = static_cast<IKsPropertySet*>(this);
    if (lo == reinterpret_cast<__int64 const*>(&IID_IKsPropertySet)[0] && hi == reinterpret_cast<__int64 const*>(&IID_IKsPropertySet)[1]) {
        goto exit;
    }
    // lastly, try the external renderer
    if (!m_pInner) {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    return m_pInner->QueryInterface(riid, ppv);
exit:
    *ppv = pv;
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    UNREFERENCED_PARAMETER(ulRef);
    return NOERROR;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CMacrovisionKicker::AddRef()
{
    // based on CUnknown::NonDelegatingAddRef()
    // the original CUnknown::NonDelegatingAddRef() has a version that keeps compatibility for Windows 95, Windows NT 3.51 and earlier, this one doesn't
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    return ulRef;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CMacrovisionKicker::Release()
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

        delete this;
        return 0;
    } else {
        // Don't touch the counter again even in this leg as the object
        // may have just been released on another thread too
        return ulRef;
    }
}

// IKsPropertySet

__declspec(nothrow noalias) STDMETHODIMP CMacrovisionKicker::Set(REFGUID PropSet, ULONG Id, LPVOID pInstanceData, ULONG InstanceLength, LPVOID pPropertyData, ULONG DataLength)
{
    IKsPropertySet* pKsPS;
    if (SUCCEEDED(m_pInner->QueryInterface(IID_IKsPropertySet, reinterpret_cast<void**>(&pKsPS)))) {
        HRESULT hr;
        if (PropSet == AM_KSPROPSETID_CopyProt && Id == AM_PROPERTY_COPY_MACROVISION) {
            TRACE(L"Oops, no-no-no, no macrovision please\n");
            hr = S_OK;
        } else { hr = pKsPS->Set(PropSet, Id, pInstanceData, InstanceLength, pPropertyData, DataLength); }
        pKsPS->Release();
        return hr;
    }
    return E_UNEXPECTED;
}

__declspec(nothrow noalias) STDMETHODIMP CMacrovisionKicker::Get(REFGUID PropSet, ULONG Id, LPVOID pInstanceData, ULONG InstanceLength, LPVOID pPropertyData, ULONG DataLength, ULONG* pBytesReturned)
{
    IKsPropertySet* pKsPS;
    if (SUCCEEDED(m_pInner->QueryInterface(IID_IKsPropertySet, reinterpret_cast<void**>(&pKsPS)))) {
        HRESULT hr = pKsPS->Get(PropSet, Id, pInstanceData, InstanceLength, pPropertyData, DataLength, pBytesReturned);
        pKsPS->Release();
        return hr;
    }
    return E_UNEXPECTED;
}

__declspec(nothrow noalias) STDMETHODIMP CMacrovisionKicker::QuerySupported(REFGUID PropSet, ULONG Id, ULONG* pTypeSupport)
{
    IKsPropertySet* pKsPS;
    if (SUCCEEDED(m_pInner->QueryInterface(IID_IKsPropertySet, reinterpret_cast<void**>(&pKsPS)))) {
        HRESULT hr = pKsPS->QuerySupported(PropSet, Id, pTypeSupport);
        pKsPS->Release();
        return hr;
    }
    return E_UNEXPECTED;
}
