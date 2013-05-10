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
#include <d3d9.h>

// CDX9SubPicAllocator

class CDX9SubPicAllocator : public CSubPicAllocatorImpl
{
    IDirect3DDevice9* m_pD3DDev;// doesn't hold a reference

    // CSubPicAllocatorImpl
    __declspec(nothrow noalias restrict) CBSubPic* Alloc(__in bool fStatic) const;

public:
    __declspec(nothrow noalias) __forceinline CDX9SubPicAllocator(__in unsigned __int32 u32Width, __in unsigned __int32 u32Height, __inout IDirect3DDevice9* pD3DDev)
        : CSubPicAllocatorImpl(u32Width, u32Height, true)
        , m_pD3DDev(pD3DDev) {
        ASSERT(pD3DDev);
    }
    __declspec(nothrow noalias) void SetDevice(__inout IDirect3DDevice9* pD3DDev) {
        m_pD3DDev = pD3DDev;
    }
};

// CDX9SubPic

class CDX9SubPic : public CBSubPic
{
    __declspec(nothrow noalias) __forceinline ~CDX9SubPic() {
        if (m_pSurface) {
            m_pSurface->Release();
        }
        if (m_pTexture) {
            m_pTexture->Release();
        }
    }

    IDirect3DSurface9* m_pSurface;
public:
    IDirect3DTexture9* m_pTexture;
    IDirect3DDevice9* m_pD3DDev;// doesn't hold a reference

    // CBSubPic
    __declspec(nothrow noalias) void GetDesc(__out SubPicDesc* pTarget) const;
    __declspec(nothrow noalias) HRESULT CopyTo(__out_opt CBSubPic* pSubPic) const;
    __declspec(nothrow noalias) HRESULT LockAndClearDirtyRect(__out_opt SubPicDesc* pTarget);
    __declspec(nothrow noalias) void Unlock(__in RECT const rDirtyRect);

    // CPU memory texture holder
    __declspec(nothrow noalias) __forceinline CDX9SubPic(__in unsigned __int32 u32Width, __in unsigned __int32 u32Height, __inout IDirect3DTexture9* pTexture)
        : CBSubPic(u32Width, u32Height)
        , m_pTexture(pTexture)
        , m_pD3DDev(nullptr) {
        ASSERT(pTexture);

        // no AddRef on m_pTexture here, the incoming texture has one reference which this class directly gains ownership of
        HRESULT hr;
        EXECUTE_ASSERT(SUCCEEDED(hr = m_pTexture->GetSurfaceLevel(0, &m_pSurface)));
    }
    // GPU memory texture holder
    __declspec(nothrow noalias) __forceinline CDX9SubPic(__in unsigned __int32 u32Width, __in unsigned __int32 u32Height, __inout IDirect3DDevice9* pD3DDev)
        : CBSubPic(u32Width, u32Height)
        , m_pSurface(nullptr)
        , m_pTexture(nullptr)
        , m_pD3DDev(pD3DDev) {
        ASSERT(pD3DDev);
    }
};
