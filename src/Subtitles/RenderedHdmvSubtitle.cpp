/*
 * (C) 2008-2013 see Authors.txt
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
#include "HdmvSub.h"
#include "RenderedHdmvSubtitle.h"

CRenderedHdmvSubtitle::CRenderedHdmvSubtitle(CCritSec* pLock, SUBTITLE_TYPE nType, const CString& name, LCID lcid)
    : CSubPicProviderImpl(pLock)
    , m_name(name)
    , m_lcid(lcid)
{
    switch (nType) {
        case ST_DVB:
            m_pSub = DEBUG_NEW CDVBSub();
            if (name.IsEmpty() || (name == _T("Unknown"))) {
                m_name = "DVB Embedded Subtitle";
            }
            break;
        case ST_HDMV:
            m_pSub = DEBUG_NEW CHdmvSub();
            if (name.IsEmpty() || (name == _T("Unknown"))) {
                m_name = "HDMV Embedded Subtitle";
            }
            break;
        default:
            ASSERT(FALSE);
            m_pSub = nullptr;
    }
    m_rtStart = 0;
}

CRenderedHdmvSubtitle::~CRenderedHdmvSubtitle()
{
    m_pSub->Destructor();
    delete m_pSub;
}

// IUnknown

__declspec(nothrow noalias) STDMETHODIMP CRenderedHdmvSubtitle::QueryInterface(REFIID riid, __deref_out void** ppv)
{
    ASSERT(ppv);
    if (riid == IID_IUnknown) { *ppv = static_cast<IUnknown*>(static_cast<CSubPicProviderImpl*>((this))); }// CSubPicProviderImpl is at Vtable location 0
    else if (riid == __uuidof(CSubPicProviderImpl)) { *ppv = static_cast<CSubPicProviderImpl*>(this); }
    else if (riid == IID_IPersist) { *ppv = static_cast<IPersist*>(this); }
    else if (riid == __uuidof(ISubStream)) { *ppv = static_cast<ISubStream*>(this); }
    else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    UNREFERENCED_PARAMETER(ulRef);
    return NOERROR;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CRenderedHdmvSubtitle::AddRef()
{
    // based on CUnknown::NonDelegatingAddRef()
    // the original CUnknown::NonDelegatingAddRef() has a version that keeps compatibility for Windows 95, Windows NT 3.51 and earlier, this one doesn't
    ULONG ulRef = _InterlockedIncrement(reinterpret_cast<LONG volatile*>(&mv_ulReferenceCount));
    ASSERT(ulRef);
    return ulRef;
}

__declspec(nothrow noalias) STDMETHODIMP_(ULONG) CRenderedHdmvSubtitle::Release()
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

// CSubPicProviderImpl

__declspec(nothrow noalias restrict) POSITION CRenderedHdmvSubtitle::GetStartPosition(__in __int64 i64Time, __in double fps)
{
    return m_pSub->GetStartPosition(i64Time - m_rtStart, fps);
}

__declspec(nothrow noalias restrict) POSITION CRenderedHdmvSubtitle::GetNext(__in POSITION pos) const
{
    return m_pSub->GetNext(pos);
}

__declspec(nothrow noalias) __int64 CRenderedHdmvSubtitle::GetStart(__in POSITION pos, __in double fps) const
{
    return m_pSub->GetStart(pos) + m_rtStart;
}

__declspec(nothrow noalias) __int64 CRenderedHdmvSubtitle::GetStop(__in POSITION pos, __in double fps) const
{
    return m_pSub->GetStop(pos) + m_rtStart;
}

__declspec(nothrow noalias) bool CRenderedHdmvSubtitle::IsAnimated(__in POSITION pos) const
{
    return false;
}

__declspec(nothrow noalias) HRESULT CRenderedHdmvSubtitle::Render(__inout SubPicDesc& spd, __in __int64 i64Time, __in double fps, __out_opt RECT& bbox)
{
    CAutoLock cAutoLock(&m_csCritSec);
    m_pSub->Render(spd, i64Time - m_rtStart, fps, bbox);

    return S_OK;
}

__declspec(nothrow noalias) unsigned __int64 CRenderedHdmvSubtitle::GetTextureSize(__in POSITION pos) const
{
    return m_pSub->GetTextureSize(pos);
};

// IPersist

STDMETHODIMP CRenderedHdmvSubtitle::GetClassID(CLSID* pClassID)
{
    return pClassID ? *pClassID = __uuidof(this), S_OK : E_POINTER;
}

// ISubStream

__declspec(nothrow noalias) size_t CRenderedHdmvSubtitle::GetStreamCount() const
{
    return 1;
}

__declspec(nothrow noalias) HRESULT CRenderedHdmvSubtitle::GetStreamInfo(__in size_t upStream, __out_opt WCHAR** ppName, __out_opt LCID* pLCID) const
{
    if (upStream != 0) {
        return E_INVALIDARG;
    }

    if (ppName) {
        *ppName = (WCHAR*)CoTaskMemAlloc((m_name.GetLength() + 1) * sizeof(WCHAR));
        if (!(*ppName)) {
            return E_OUTOFMEMORY;
        }

        wcscpy_s(*ppName, m_name.GetLength() + 1, CStringW(m_name));
    }

    if (pLCID) {
        *pLCID = m_lcid;
    }

    return S_OK;
}

__declspec(nothrow noalias) size_t CRenderedHdmvSubtitle::GetStream() const
{
    return 0;
}

__declspec(nothrow noalias) HRESULT CRenderedHdmvSubtitle::SetStream(__in size_t uStream)
{
    return (uStream == 0) ? S_OK : E_FAIL;
}

__declspec(nothrow noalias) HRESULT CRenderedHdmvSubtitle::Reload()
{
    return S_OK;
}

__declspec(nothrow noalias) HRESULT CRenderedHdmvSubtitle::ParseSample(__inout IMediaSample* pSample)
{
    CAutoLock cAutoLock(&m_csCritSec);
    HRESULT hr;

    hr = m_pSub->ParseSample(pSample);
    return hr;
}

__declspec(nothrow noalias) void CRenderedHdmvSubtitle::EndOfStream()
{
    CAutoLock cAutoLock(&m_csCritSec);
    m_pSub->EndOfStream();
}

HRESULT CRenderedHdmvSubtitle::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    CAutoLock cAutoLock(&m_csCritSec);

    m_pSub->Reset();
    m_rtStart = tStart;
    return S_OK;
}
