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

#include "DX9AllocatorPresenter.h"
#include "IQTVideoSurface.h"

namespace DSObjects
{
    class CQT9AllocatorPresenter
        : public CDX9AllocatorPresenter
        , public IQTVideoSurface
    {
        __declspec(nothrow noalias) __forceinline ~CQT9AllocatorPresenter() {
            if (m_pVideoSurfaceOff) {
                ULONG u = m_pVideoSurfaceOff->Release();
                ASSERT(!u);
            }
        }

        IDirect3DSurface9* m_pVideoSurfaceOff;

    public:
        // IUnknown
        __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv);
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef();
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release();

        // CSubPicAllocatorPresenterImpl
        __declspec(nothrow noalias) void ResetDevice();

        // IQTVideoSurface
        __declspec(nothrow noalias) STDMETHODIMP BeginBlt(BITMAP const& bm);
        __declspec(nothrow noalias) STDMETHODIMP DoBlt(BITMAP const& bm);

        __declspec(nothrow noalias) __forceinline CQT9AllocatorPresenter(__in HWND hWnd, __inout CStringW* pstrError)
            : CDX9AllocatorPresenter(hWnd, pstrError, false)
            , m_pVideoSurfaceOff(nullptr) {
            ASSERT(pstrError);

            m_u8MixerSurfaceCount = mk_pRendererSettings->MixerBuffers;// set the number of buffers
            // note; when expanding this function, handle the case for when CDX9AllocatorPresenter() fails
        }
    };
}
