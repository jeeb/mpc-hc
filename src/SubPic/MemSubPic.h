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

#include "ISubPic.h"

__declspec(nothrow noalias) void alpha_blt_rgb32(__in const RECT rcSrc, __in const RECT rcDst, __in const SubPicDesc* pTarget, __in const SubPicDesc& src);

enum {// note that the order is important here, RGB/YUV and the levels of chroma sub-sampling detection should stay properly ordered
    // RGB types
    MSP_RGBA = 0,// this has to be the default = 0 type
    MSP_RGB32,
    MSP_RGB24,
    MSP_RGB16,
    MSP_RGB15,
    // YUV types
    // 4:4:4
    MSP_AYUV,
    // 4:2:2
    MSP_YUY2,
    // 4:2:0
    MSP_YV12,
    MSP_IYUV,// note: I420 is the same
    MSP_NV12,
    MSP_NV21
};

// CMemSubPic

class CMemSubPic : public CBSubPic
{
    __declspec(nothrow noalias) __forceinline ~CMemSubPic() {
        _aligned_free(m_spd.bits);
    }
public:
    SubPicDesc m_spd;

    // CBSubPic
    __declspec(nothrow noalias) void GetDesc(__out SubPicDesc* pTarget) const;
    __declspec(nothrow noalias) HRESULT CopyTo(__out_opt CBSubPic* pSubPic) const;
    __declspec(nothrow noalias) HRESULT LockAndClearDirtyRect(__out_opt SubPicDesc* pTarget);
    __declspec(nothrow noalias) void Unlock(__in RECT const rDirtyRect);

    __declspec(nothrow noalias) __forceinline CMemSubPic(SubPicDesc const& spd)
        : CBSubPic(spd.w, spd.h)
        , m_spd(spd) {}
};

// CMemSubPicAllocator

class CMemSubPicAllocator : public CSubPicAllocatorImpl
{
    unsigned __int8 const mk_type;

    // CSubPicAllocatorImpl
    __declspec(nothrow noalias restrict) CBSubPic* Alloc(__in bool fStatic) const;

public:
    __declspec(nothrow noalias) __forceinline CMemSubPicAllocator(__in unsigned __int32 u32Width, __in unsigned __int32 u32Height, __in unsigned __int8 u8Type)
        : CSubPicAllocatorImpl(u32Width, u32Height, false)
        , mk_type(u8Type) {}
};
