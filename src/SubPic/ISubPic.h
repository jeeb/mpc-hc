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

#include <atlbase.h>
#include <atlcoll.h>
#include "SubPicAllocatorPresenterImpl.h"

//
// ISubStream
//
interface __declspec(uuid("DE11E2FB-02D3-45E4-A174-6B7CE2783BDB") novtable)
ISubStream :
public IPersist {
    virtual __declspec(nothrow noalias) size_t GetStreamCount() const = 0;
    virtual __declspec(nothrow noalias) HRESULT GetStreamInfo(__in size_t upStream, __out_opt WCHAR** ppName, __out_opt LCID * pLCID) const = 0;// __out_opt used for when StreamInfo isn't available, and HRESULT returns FAILED
    virtual __declspec(nothrow noalias) size_t GetStream() const = 0;
    virtual __declspec(nothrow noalias) HRESULT SetStream(__in size_t upStream) = 0;
    virtual __declspec(nothrow noalias) HRESULT Reload() = 0;

    // TODO: get rid of IPersist to identify type and use only
    // interface functions to modify the settings of the substream
};

//
// IBaseSub
//
// interface for CDVBSub and CRenderedHdmvSubtitle
enum SUBTITLE_TYPE {
    ST_DVB,
    ST_HDMV
};

interface __declspec(novtable)// not a COM interface
IBaseSub {
    virtual __declspec(nothrow noalias) void Destructor() = 0;
    virtual __declspec(nothrow noalias) HRESULT ParseSample(__inout IMediaSample * pSample) = 0;
    virtual __declspec(nothrow noalias) void Reset() = 0;
    virtual __declspec(nothrow noalias restrict) POSITION GetStartPosition(__in __int64 rt, __in double fps) = 0;
    virtual __declspec(nothrow noalias restrict) POSITION GetNext(__in POSITION pos) const = 0;
    virtual __declspec(nothrow noalias) __int64 GetStart(__in POSITION nPos) const = 0;
    virtual __declspec(nothrow noalias) __int64 GetStop(__in POSITION nPos) const = 0;
    virtual __declspec(nothrow noalias) void EndOfStream();
    virtual __declspec(nothrow noalias) void Render(__inout SubPicDesc & spd, __in __int64 rt, __in double fps, __out_opt RECT & bbox) = 0;
    virtual __declspec(nothrow noalias) unsigned __int64 GetTextureSize(__in POSITION pos) const = 0;
};
