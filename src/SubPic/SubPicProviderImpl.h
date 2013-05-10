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

#include "SubPicImpl.h"

class __declspec(uuid("D62B9A1A-879A-42DB-AB04-88AA8F243CFD") novtable) CSubPicProviderImpl
    : public IUnknown
{
    // polymorphic class not implementing IUnknown, so no virtual destructor required
protected:
    CCritSec* m_pLock;
    ULONG volatile mv_ulReferenceCount;

public:
    virtual __declspec(nothrow noalias restrict) POSITION GetStartPosition(__in __int64 i64Time, __in double fps) = 0;
    virtual __declspec(nothrow noalias restrict) POSITION GetNext(__in POSITION pos) const = 0;
    virtual __declspec(nothrow noalias) __int64 GetStart(__in POSITION pos, __in double fps) const = 0;
    virtual __declspec(nothrow noalias) __int64 GetStop(__in POSITION pos, __in double fps) const = 0;
    virtual __declspec(nothrow noalias) bool IsAnimated(__in POSITION pos) const = 0;
    virtual __declspec(nothrow noalias) HRESULT Render(__inout SubPicDesc& spd, __in __int64 i64Time, __in double fps, __out_opt RECT& bbox) = 0;// __out_opt used for when rendering fails, and HRESULT returns FAILED
    virtual __declspec(nothrow noalias) unsigned __int64 GetTextureSize(__in POSITION pos) const {// width in the low 32 bits, height in the high 32 bits
        return 0;// only implemented in CHdmvSub and CDVBSub though their IBaseSub part
    }

    __declspec(nothrow noalias) __forceinline CSubPicProviderImpl(__in CCritSec* pLock)
        : m_pLock(pLock)
        , mv_ulReferenceCount(0) {
        ASSERT(pLock);
    }
    __declspec(nothrow noalias) __forceinline void Lock() { m_pLock->Lock(); }
    __declspec(nothrow noalias) __forceinline void Unlock() { m_pLock->Unlock(); }
};
