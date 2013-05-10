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

#include "SubPicQueueImpl.h"
#include "CoordGeom.h"
#include "../filters/renderer/VideoRenderers/RenderersSettings.h"

class __declspec(uuid("CF75B1F0-535C-4074-8869-B15F177F944E") novtable) CSubPicAllocatorPresenterImpl
    : public IUnknown
{
    // polymorphic class not implementing IUnknown, so no virtual destructor required

protected:
    // section begin of 16-byte aligned requirements, this is tested in the constructor function at compile time
    // alignment: one hidden pointer here

    // these three items are constructed by each renderer and each renderer should destroy them when appropriate
    // because this class is a base, its destructor is called late, so the resources possibly needed to release these three items may already be detached by the renderer's destructor
    // hence, this class doesn't have a destructor
    CSubPicQueueImpl* m_pSubPicQueue;
    CSubPicAllocatorImpl* m_pSubPicAllocator;
    CSubPicProviderImpl* m_pSubPicProvider;

    unsigned __int32 m_u32VideoWidth, m_u32VideoHeight, m_u32AspectRatioWidth, m_u32AspectRatioHeight;
    RECT m_VideoRect;
    // see class initializer for the reason these four are placed together
    __int32 m_i32SubWindowOffsetLeft, m_i32SubWindowOffsetTop;
    unsigned __int32 m_u32WindowWidth, m_u32WindowHeight;
    // section end of aligned requirements

    XForm m_xform;
    ULONG volatile mv_ulReferenceCount;
    __int64 m_i64SubtitleDelay;
    __int64 m_i64Now;
    double m_dDetectedVideoFrameRate;
    HWND m_hVideoWnd;
    CRenderersSettings const* const mk_pRendererSettings;

public:
    __declspec(nothrow noalias) CSubPicAllocatorPresenterImpl(__in HWND hVideoWnd);

    __declspec(nothrow noalias) void SetSubPicProvider(__inout CSubPicProviderImpl* pSubPicProvider);

    virtual __declspec(nothrow noalias) SIZE GetVideoSize(__in bool fCorrectAR) const;
    virtual __declspec(nothrow noalias) void SetPosition(__in_ecount(2) RECT const arcWV[2]);
    virtual __declspec(nothrow noalias) HRESULT GetDIB(__out_opt void* pDib, __inout size_t* pSize) = 0;// __out_opt used for when getting the DIB failed, and HRESULT returns FAILED or when only a required size is requested
    // returned value: 0 for success, 1 for an out of memory error, 2 for not implemented, else a CStringW* to the label of the failed shader
    virtual __declspec(nothrow noalias) uintptr_t SetPixelShaders(__in_ecount(2) CAtlList<Shader const*> const aList[2]) {
        return 2;// CDXRAllocatorPresenter doesn't implement it
    }
    // bitwise logic on input: 1 == only stage 0, 2 == only stage 1, 3 == both
    virtual __declspec(nothrow noalias) void ClearPixelShaders(unsigned __int8 u8RenderStages) {
    }// CDXRAllocatorPresenter doesn't implement it
    virtual __declspec(nothrow noalias) void ResetDevice() {
    }// CDXRAllocatorPresenter and CmadVRAllocatorPresenter don't implement it

    __declspec(nothrow noalias) __forceinline void SetVideoAngle(__in Vector const* pv) {
        ASSERT(pv);

        m_xform = XForm(Ray(Vector(0.0, 0.0, 0.0), *pv), Vector(1.0, 1.0, 1.0), false);
    }
    __declspec(nothrow noalias) __forceinline void SetVideoWindow(__in HWND hVideoWnd) {
        ASSERT(hVideoWnd);

        m_hVideoWnd = hVideoWnd;
    }
    __declspec(nothrow noalias) __forceinline void Invalidate(__in __int64 i64Invalidate) {
        if (m_pSubPicQueue) {
            m_pSubPicQueue->InvalidateSubPic(i64Invalidate);
        }
    }
    __declspec(nothrow noalias) __forceinline double GetFPS() const {
        return m_dDetectedVideoFrameRate;
    }
    __declspec(nothrow noalias) __forceinline void SetTime(__in __int64 i64Now) {
        m_i64Now = i64Now - m_i64SubtitleDelay;

        if (m_pSubPicQueue) {
            m_pSubPicQueue->SetTime(m_i64Now);
        }
    }
    __declspec(nothrow noalias) __forceinline void SetSubtitleDelay(__in __int64 i64DelayIms) {
        m_i64SubtitleDelay = i64DelayIms * 10000;
    }
    __declspec(nothrow noalias) __forceinline __int64 GetSubtitleDelay() const {
        return m_i64SubtitleDelay / 10000;
    }
};
