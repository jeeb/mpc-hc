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
#include "ISubPic.h"
#include "../DSUtil/DSUtil.h"

__declspec(nothrow noalias) CSubPicAllocatorPresenterImpl::CSubPicAllocatorPresenterImpl(__in HWND hVideoWnd)
    : m_hVideoWnd(hVideoWnd)
    , mk_pRendererSettings(GetRenderersSettings())
    , mv_ulReferenceCount(1)
    , m_u32VideoWidth(0), m_u32VideoHeight(0)// many parts will test wideo width and height for initialization
    , m_u32AspectRatioWidth(0), m_u32AspectRatioHeight(0)// GetVideoSize() tests for this
    , m_i64SubtitleDelay(0)
    , m_i64Now(0)
    , m_pSubPicQueue(nullptr)
    , m_pSubPicAllocator(nullptr)
    , m_pSubPicProvider(nullptr)
    , m_dDetectedVideoFrameRate(25.0)// common default for video and subtitle renderer
{
    static_assert(!(offsetof(CSubPicAllocatorPresenterImpl, m_u32VideoWidth) & 15), "structure alignment test failed, edit this class to correct the issue");
    ASSERT(m_hVideoWnd);

    // rough initialization of the window and video areas, will be overwritten later on
    RECT rc;
    EXECUTE_ASSERT(GetWindowRect(m_hVideoWnd, &rc));
    LONG ww = rc.right - rc.left;
    if (!ww) {
        ww = 1;
    }
    LONG wh = rc.bottom - rc.top;
    if (!wh) {
        wh = 1;
    }
    *reinterpret_cast<__int64*>(&m_VideoRect) = 0;// clear left and top
    m_u32WindowWidth = m_VideoRect.right = ww;
    m_u32WindowHeight = m_VideoRect.bottom = wh;
}

__declspec(nothrow noalias) SIZE CSubPicAllocatorPresenterImpl::GetVideoSize(__in bool fCorrectAR) const
{
    SIZE VideoSize = {m_u32VideoWidth, m_u32VideoHeight};

    if (fCorrectAR && m_u32AspectRatioWidth && m_u32AspectRatioHeight) {
        VideoSize.cx = m_u32VideoHeight * m_u32AspectRatioWidth / m_u32AspectRatioHeight;
    }

    return VideoSize;
}

__declspec(nothrow noalias) void CSubPicAllocatorPresenterImpl::SetPosition(__in_ecount(2) RECT const arcWV[2])
{
    ASSERT(arcWV);
    ASSERT(!arcWV[0].left && !arcWV[0].top);// verify the window rectangle top-left zero base offset
    ASSERT((arcWV[0].right > 0) && (arcWV[0].bottom > 0));// verify the window rectangle dimensions
    ASSERT((arcWV[1].right - arcWV[1].left > 0) && (arcWV[1].bottom - arcWV[1].top > 0));// verify the video rectangle dimensions

    if ((static_cast<LONG>(m_u32WindowWidth) != arcWV[0].right) || (static_cast<LONG>(m_u32WindowHeight) != arcWV[0].bottom)
            || (m_VideoRect.left != arcWV[1].left) || (m_VideoRect.top != arcWV[1].top) || (m_VideoRect.right != arcWV[1].right) || (m_VideoRect.bottom != arcWV[1].bottom)) {
        m_u32WindowWidth = arcWV[0].right;
        m_u32WindowHeight = arcWV[0].bottom;
        m_VideoRect = arcWV[1];
        if (m_pSubPicAllocator) {
            // create subtitle renderer resources
            __int32 i32Oleft = 0, i32Otop = 0;
            unsigned __int32 u32Width = arcWV[0].right, u32Height = arcWV[0].bottom;
            if (mk_pRendererSettings->bPositionRelative) {
                i32Oleft = arcWV[1].left;
                i32Otop = arcWV[1].top;
                u32Width = arcWV[1].right - arcWV[1].left;
                u32Height = arcWV[1].bottom - arcWV[1].top;
            }
            if (mk_pRendererSettings->nSPCMaxRes) { // half and tree-quarters resolution
                i32Oleft >>= 1;
                i32Otop >>= 1;
                u32Width >>= 1;
                u32Height >>= 1;
                if (mk_pRendererSettings->nSPCMaxRes == 1) { // tree-quarters resolution
                    i32Oleft += i32Oleft >> 1;
                    i32Otop += i32Otop >> 1;
                    u32Width += u32Width >> 1;
                    u32Height += u32Height >> 1;
                }
            }
            m_i32SubWindowOffsetLeft = i32Oleft;
            m_i32SubWindowOffsetTop = i32Otop;
            m_pSubPicAllocator->SetCurSize(static_cast<unsigned __int64>(u32Width) | static_cast<unsigned __int64>(u32Height) << 32);
        }
        if (m_pSubPicQueue) {
            m_pSubPicQueue->InvalidateSubPic(-1);
        }
    }
}

__declspec(nothrow noalias) void CSubPicAllocatorPresenterImpl::SetSubPicProvider(__inout CSubPicProviderImpl* pSubPicProvider)
{
    if (m_pSubPicProvider) {
        m_pSubPicProvider->Release();
    }
    m_pSubPicProvider = pSubPicProvider;
    if (pSubPicProvider) {
        pSubPicProvider->AddRef();

        if (m_pSubPicAllocator) {
            // create subtitle renderer resources
            __int32 i32Oleft = 0, i32Otop = 0;
            unsigned __int32 u32Width = m_u32WindowWidth, u32Height = m_u32WindowHeight;
            if (mk_pRendererSettings->bPositionRelative) {
                i32Oleft = m_VideoRect.left;
                i32Otop = m_VideoRect.top;
                u32Width = m_VideoRect.right - m_VideoRect.left;
                u32Height = m_VideoRect.bottom - m_VideoRect.top;
            }
            if (mk_pRendererSettings->nSPCMaxRes) { // half and tree-quarters resolution
                i32Oleft >>= 1;
                i32Otop >>= 1;
                u32Width >>= 1;
                u32Height >>= 1;
                if (mk_pRendererSettings->nSPCMaxRes == 1) { // tree-quarters resolution
                    i32Oleft += i32Oleft >> 1;
                    i32Otop += i32Otop >> 1;
                    u32Width += u32Width >> 1;
                    u32Height += u32Height >> 1;
                }
            }
            m_i32SubWindowOffsetLeft = i32Oleft;
            m_i32SubWindowOffsetTop = i32Otop;
            m_pSubPicAllocator->SetCurSize(static_cast<unsigned __int64>(u32Width) | static_cast<unsigned __int64>(u32Height) << 32);
        }
    }
    if (m_pSubPicQueue) {
        m_pSubPicQueue->SetSubPicProvider(pSubPicProvider);// also calls InvalidateSubPic
    }
}
