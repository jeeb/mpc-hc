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

class CMacrovisionKicker
    : public IKsPropertySet
{
    __declspec(nothrow noalias) __forceinline ~CMacrovisionKicker() {
        if (m_pInner) {
            m_pInner->Release();
        }
    }

    IUnknown* m_pInner;
    ULONG volatile mv_ulReferenceCount;
public:
    __declspec(nothrow noalias) __forceinline CMacrovisionKicker()
        : m_pInner(nullptr)
        , mv_ulReferenceCount(1) {}
    __declspec(nothrow noalias) __forceinline void SetInner(IUnknown* pUnk) {
        m_pInner = pUnk;
        pUnk->AddRef();
    }

    // IUnknown
    __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv);
    __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef();
    __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release();

    // IKsPropertySet
    __declspec(nothrow noalias) STDMETHODIMP Set(REFGUID PropSet, ULONG Id, LPVOID pInstanceData, ULONG InstanceLength, LPVOID pPropertyData, ULONG DataLength);
    __declspec(nothrow noalias) STDMETHODIMP Get(REFGUID PropSet, ULONG Id, LPVOID pInstanceData, ULONG InstanceLength, LPVOID pPropertyData, ULONG DataLength, ULONG* pBytesReturned);
    __declspec(nothrow noalias) STDMETHODIMP QuerySupported(REFGUID PropSet, ULONG Id, ULONG* pTypeSupport);
};
