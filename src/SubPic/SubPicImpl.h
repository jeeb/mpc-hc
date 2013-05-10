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

struct SubPicDesc {
    size_t w;
    ptrdiff_t h;
    size_t pitch, pitchUV;
    void* bits;
    BYTE* bitsU;
    BYTE* bitsV;
    RECT vidrect;// video rectangle
    unsigned __int8 type, bpp;
};

class __declspec(uuid("449E11F3-52D1-4A27-AA61-E2733AC92CC0") novtable) CBSubPic
    : public IUnknown
{
protected:
    virtual __declspec(nothrow noalias) __forceinline ~CBSubPic() {}// polymorphic class implementing IUnknown, so a virtual destructor

    REFERENCE_TIME m_rtStart, m_rtStop;
    REFERENCE_TIME m_rtSegmentStart, m_rtSegmentStop;
    ULONG volatile mv_ulReferenceCount;
    RECT    m_rcDirty;
    RECT    m_vidrect;
    SIZE    m_maxsize;

public:
    // IUnknown
    __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv);
    __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef();
    __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release();

    __declspec(nothrow noalias) bool GetSourceAndDest(__out_ecount_opt(2) RECT arcSourceDest[2]) const;// __out_opt used for when there's no subtitle to be displayed, and the function returns false

    virtual __declspec(nothrow noalias) void GetDesc(__out SubPicDesc* pTarget) const = 0;
    virtual __declspec(nothrow noalias) HRESULT CopyTo(__out_opt CBSubPic* pSubPic) const = 0;// __out_opt used for when referenced objects don't exist, and HRESULT returns FAILED
    virtual __declspec(nothrow noalias) HRESULT LockAndClearDirtyRect(__out_opt SubPicDesc* pTarget) = 0;// __out_opt used for when the lock fails, and HRESULT returns FAILED
    virtual __declspec(nothrow noalias) void Unlock(__in RECT const rDirtyRect) = 0;

    __declspec(nothrow noalias) __forceinline CBSubPic(__in unsigned __int32 u32Width, __in unsigned __int32 u32Height)
        : mv_ulReferenceCount(1)
        , m_rtStart(0)
        , m_rtStop(0)
        , m_rtSegmentStart(0)
        , m_rtSegmentStop(0) {
        m_vidrect.left = 0;
        m_vidrect.top = 0;
        m_vidrect.right = u32Width;
        m_vidrect.bottom = u32Height;
        m_maxsize.cx = u32Width;
        m_maxsize.cy = u32Height;
        m_rcDirty.left = 0;
        m_rcDirty.top = 0;
        m_rcDirty.right = u32Width;
        m_rcDirty.bottom = u32Height;
    }
    __declspec(nothrow noalias) __forceinline __int64 GetStart() const {
        return m_rtStart;
    }
    __declspec(nothrow noalias) __forceinline __int64 GetStop() const {
        return m_rtStop;
    }
    __declspec(nothrow noalias) __forceinline void SetStart(__in __int64 rtStart) {
        m_rtStart = rtStart;
    }
    __declspec(nothrow noalias) __forceinline void SetStop(__in __int64 rtStop) {
        m_rtStop = rtStop;
    }
    __declspec(nothrow noalias) __forceinline RECT GetDirtyRect() const {
        return m_rcDirty;
    }
    __declspec(nothrow noalias) __forceinline void SetDirtyRect(__in RECT const* pDirtyRect) {
        m_rcDirty = *pDirtyRect;
    }
    __declspec(nothrow noalias) __forceinline __int64 GetSegmentStart() const {
        if (m_rtSegmentStart) {
            return m_rtSegmentStart;
        }
        return m_rtStart;
    }
    __declspec(nothrow noalias) __forceinline __int64 GetSegmentStop() const {
        if (m_rtSegmentStop) {
            return m_rtSegmentStop;
        }
        return m_rtStop;
    }
    __declspec(nothrow noalias) __forceinline void SetSegmentStart(__in __int64 rtStart) {
        m_rtSegmentStart = rtStart;
    }
    __declspec(nothrow noalias) __forceinline void SetSegmentStop(__in __int64 rtStop) {
        m_rtSegmentStop = rtStop;
    }
};

class __declspec(uuid("CF7C3C23-6392-4a42-9E72-0736CFF793CB") novtable) CSubPicAllocatorImpl
    : public IUnknown
{
protected:
    __declspec(nothrow noalias) __forceinline ~CSubPicAllocatorImpl() {// polymorphic class implementing IUnknown, so a virtual destructor would be in place, were it not that both CDX9SubPicAllocator and CMemSubPicAllocator are designed to have no destructor and no destructible objects in the class
        if (mv_pStatic) {
            mv_pStatic->Release();
        }
    }

private:
    virtual __declspec(nothrow noalias restrict) CBSubPic* Alloc(__in bool fStatic) const = 0;

    CBSubPic* volatile mv_pStatic;
protected:
    __declspec(align(8)) unsigned __int32 m_u32Width;// m_u32Width and m_u32Height are written as a pair in SetCurSize()
    unsigned __int32  m_u32Height;
private:
    ULONG volatile mv_ulReferenceCount;
    bool const mk_bDynamicWriteOnly;

public:
    // IUnknown
    __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv);
    __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef();
    __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release();

    __declspec(nothrow noalias) __forceinline CSubPicAllocatorImpl(__in unsigned __int32 u32Width, __in unsigned __int32 u32Height, __in bool bDynamicWriteOnly)
        : mv_ulReferenceCount(1)
        , mk_bDynamicWriteOnly(bDynamicWriteOnly)
        , mv_pStatic(nullptr)
        , m_u32Width(u32Width)
        , m_u32Height(u32Height) {}
    __declspec(nothrow noalias) __forceinline void DeallocStaticSubPic() {
        if (CBSubPic* pOld = reinterpret_cast<CBSubPic*>(InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&mv_pStatic), nullptr))) {
            pOld->Release();
        }
    }
    __declspec(nothrow noalias) __forceinline void SetCurSize(__in unsigned __int64 u64WidthAndHeight) {// packed input, width in the low 32 bits, height in the high 32 bits
        if (*reinterpret_cast<unsigned __int64*>(&m_u32Width) != u64WidthAndHeight) {
            InterlockedExchange64(reinterpret_cast<__int64*>(&m_u32Width), u64WidthAndHeight);// m_u32Width and m_u32Height do not need to be treated as volatile per se, as only the constructor and this routine ever write these values
            if (CBSubPic* pOld = reinterpret_cast<CBSubPic*>(InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&mv_pStatic), nullptr))) {
                pOld->Release();
            }
        }
    }
    __declspec(nothrow noalias) __forceinline bool IsDynamicWriteOnly() const {
        return mk_bDynamicWriteOnly;
    }
    __declspec(nothrow noalias restrict) __forceinline CBSubPic* AllocStaticSubPic() {// gives only a reference to the caller
        CBSubPic* pBSubPic = mv_pStatic;
        if (!pBSubPic) {
            pBSubPic = Alloc(true);
            if (!pBSubPic) {
                return nullptr;
            }
            if (CBSubPic* pOld = reinterpret_cast<CBSubPic*>(InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&mv_pStatic), pBSubPic))) {
                pOld->Release();
            }
        }
        pBSubPic->AddRef();
        return pBSubPic;
    }
    __declspec(nothrow noalias restrict) __forceinline CBSubPic* AllocDynamicSubPic() const {// gives full ownership to the caller
        return Alloc(false);
    }
};
