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
#include <vmr9.h>

namespace DSObjects
{
    class COuterEVR// note: this class holds the final reference before destruction of CEVRAllocatorPresenter, see COuterEVR::Release() for reference
        : public IBaseFilter
        , public IKsPropertySet
    {
    public:
        IUnknown* m_pEVR;
        IBaseFilter* m_pBaseFilter;// does not hold a reference inside this class
        ULONG volatile mv_ulReferenceCount;

        __declspec(nothrow noalias) __forceinline COuterEVR() : mv_ulReferenceCount(0) {}// the first reference is created by CEVRAllocatorPresenter

        // IUnknown
        __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv);
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef();
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release();

        // IBaseFilter
        __declspec(nothrow noalias) STDMETHODIMP EnumPins(__out IEnumPins** ppEnum);
        __declspec(nothrow noalias) STDMETHODIMP FindPin(LPCWSTR Id, __out IPin** ppPin);
        __declspec(nothrow noalias) STDMETHODIMP QueryFilterInfo(__out FILTER_INFO* pInfo);
        __declspec(nothrow noalias) STDMETHODIMP JoinFilterGraph(__in_opt IFilterGraph* pGraph, __in_opt LPCWSTR pName);
        __declspec(nothrow noalias) STDMETHODIMP QueryVendorInfo(__out LPWSTR* pVendorInfo);

        // IMediaFilter
        __declspec(nothrow noalias) STDMETHODIMP Stop();
        __declspec(nothrow noalias) STDMETHODIMP Pause();
        __declspec(nothrow noalias) STDMETHODIMP Run(REFERENCE_TIME tStart);
        __declspec(nothrow noalias) STDMETHODIMP GetState(DWORD dwMilliSecsTimeout, __out FILTER_STATE* State);
        __declspec(nothrow noalias) STDMETHODIMP SetSyncSource(__in_opt IReferenceClock* pClock);
        __declspec(nothrow noalias) STDMETHODIMP GetSyncSource(__deref_out_opt IReferenceClock** pClock);

        // IPersist
        __declspec(nothrow noalias) STDMETHODIMP GetClassID(__RPC__out CLSID* pClassID);

        // IKsPropertySet - MacrovisionKicker
        __declspec(nothrow noalias) STDMETHODIMP Set(REFGUID PropSet, ULONG Id, LPVOID pInstanceData, ULONG InstanceLength, LPVOID pPropertyData, ULONG DataLength);
        __declspec(nothrow noalias) STDMETHODIMP Get(REFGUID PropSet, ULONG Id, LPVOID pInstanceData, ULONG InstanceLength, LPVOID pPropertyData, ULONG DataLength, ULONG* pBytesReturned);
        __declspec(nothrow noalias) STDMETHODIMP QuerySupported(REFGUID PropSet, ULONG Id, ULONG* pTypeSupport);
    };
}
