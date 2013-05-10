/*
 * (C) 2008-2012 see Authors.txt
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

#include "Rasterizer.h"
#include "../SubPic/SubPicProviderImpl.h"
#include "HdmvSub.h"
#include "DVBSub.h"


class __declspec(uuid("FCA68599-C83E-4ea5-94A3-C2E1B0E326B9"))
    CRenderedHdmvSubtitle
    : public CSubPicProviderImpl
    , public ISubStream
{
public:
    CRenderedHdmvSubtitle(CCritSec* pLock, SUBTITLE_TYPE nType, const CString& name, LCID lcid);
    ~CRenderedHdmvSubtitle();

    // IUnknown
    __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv);
    __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef();
    __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release();

    // ISubPicProvider
    __declspec(nothrow noalias restrict) POSITION GetStartPosition(__in __int64 i64Time, __in double fps);
    __declspec(nothrow noalias restrict) POSITION GetNext(__in POSITION pos) const;
    __declspec(nothrow noalias) __int64 GetStart(__in POSITION pos, __in double fps) const;
    __declspec(nothrow noalias) __int64 GetStop(__in POSITION pos, __in double fps) const;
    __declspec(nothrow noalias) bool IsAnimated(__in POSITION pos) const;
    __declspec(nothrow noalias) HRESULT Render(__inout SubPicDesc& spd, __in __int64 i64Time, __in double fps, __out_opt RECT& bbox);
    __declspec(nothrow noalias) unsigned __int64 GetTextureSize(__in POSITION pos) const;

    // IPersist
    STDMETHODIMP GetClassID(CLSID* pClassID);

    // ISubStream
    __declspec(nothrow noalias) size_t GetStreamCount() const;
    __declspec(nothrow noalias) HRESULT GetStreamInfo(__in size_t upStream, __out_opt WCHAR** ppName, __out_opt LCID* pLCID) const;
    __declspec(nothrow noalias) size_t GetStream() const;
    __declspec(nothrow noalias) HRESULT SetStream(__in size_t upStream);
    __declspec(nothrow noalias) HRESULT Reload();

    // IBaseSub
    __declspec(nothrow noalias) HRESULT ParseSample(__inout IMediaSample* pSample);
    __declspec(nothrow noalias) void EndOfStream();

    HRESULT NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);
private:
    CString         m_name;
    LCID            m_lcid;
    REFERENCE_TIME  m_rtStart;

    IBaseSub*       m_pSub;
    CCritSec        m_csCritSec;
};
